/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BCEScriptStencil.h"

#include "frontend/AbstractScopePtr.h"  // AbstractScopePtr
#include "frontend/BytecodeEmitter.h"   // BytecodeEmitter
#include "frontend/BytecodeSection.h"   // BytecodeSection, PerScriptData

using namespace js;
using namespace js::frontend;

BCEScriptStencil::BCEScriptStencil(BytecodeEmitter& bce)
    : ScriptStencil(bce.cx), bce_(bce) {}

bool BCEScriptStencil::init(JSContext* cx, uint32_t nslots) {
  lineno = bce_.firstLine;
  column = bce_.firstColumn;

  natoms = bce_.perScriptData().atomIndices()->count();

  ngcthings = bce_.perScriptData().gcThingList().length();

  bool isFunction = bce_.sc->isFunctionBox();
  uint16_t funLength = isFunction ? bce_.sc->asFunctionBox()->length : 0;

  immutableScriptData = ImmutableScriptData::new_(
      cx, bce_.mainOffset(), bce_.maxFixedSlots, nslots, bce_.bodyScopeIndex,
      bce_.bytecodeSection().numICEntries(),
      bce_.bytecodeSection().numTypeSets(), isFunction, funLength,
      bce_.bytecodeSection().code(), bce_.bytecodeSection().notes(),
      bce_.bytecodeSection().resumeOffsetList().span(),
      bce_.bytecodeSection().scopeNoteList().span(),
      bce_.bytecodeSection().tryNoteList().span());
  if (!immutableScriptData) {
    return false;
  }

  strict = bce_.sc->strict();
  bindingsAccessedDynamically = bce_.sc->bindingsAccessedDynamically();
  hasCallSiteObj = bce_.sc->hasCallSiteObj();
  isForEval = bce_.sc->isEvalContext();
  isModule = bce_.sc->isModuleContext();
  this->isFunction = isFunction;
  hasNonSyntacticScope =
      bce_.outermostScope().hasOnChain(ScopeKind::NonSyntactic);
  needsFunctionEnvironmentObjects = getNeedsFunctionEnvironmentObjects();
  hasModuleGoal = bce_.sc->hasModuleGoal();
  hasInnerFunctions = bce_.sc->hasInnerFunctions();

  gcThings = bce_.perScriptData().gcThingList().stealGCThings();

  if (isFunction) {
    functionBox = bce_.sc->asFunctionBox();
  }
  return true;
}

bool BCEScriptStencil::getNeedsFunctionEnvironmentObjects() const {
  // See JSFunction::needsCallObject()
  js::AbstractScopePtr bodyScope = bce_.bodyScope();
  if (bodyScope.kind() == js::ScopeKind::Function) {
    if (bodyScope.hasEnvironment()) {
      return true;
    }
  }

  // See JSScript::maybeNamedLambdaScope()
  js::AbstractScopePtr outerScope = bce_.outermostScope();
  if (outerScope.kind() == js::ScopeKind::NamedLambda ||
      outerScope.kind() == js::ScopeKind::StrictNamedLambda) {
    MOZ_ASSERT(bce_.sc->asFunctionBox()->isNamedLambda());

    if (outerScope.hasEnvironment()) {
      return true;
    }
  }

  return false;
}

bool BCEScriptStencil::finishGCThings(
    JSContext* cx, mozilla::Span<JS::GCCellPtr> output) const {
  return EmitScriptThingsVector(cx, bce_.compilationInfo, gcThings, output);
}

void BCEScriptStencil::initAtomMap(GCPtrAtom* atoms) const {
  const AtomIndexMap& indices = *bce_.perScriptData().atomIndices();

  for (AtomIndexMap::Range r = indices.all(); !r.empty(); r.popFront()) {
    JSAtom* atom = r.front().key();
    uint32_t index = r.front().value();
    MOZ_ASSERT(index < indices.count());
    atoms[index].init(atom);
  }
}

void BCEScriptStencil::finishInnerFunctions() const {
  bce_.perScriptData().gcThingList().finishInnerFunctions();
}
