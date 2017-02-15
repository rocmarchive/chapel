/*
 * Copyright 2017 Advanced Micro Devices, Inc.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// LocaleModel.chpl
//
// This provides a flat locale model architectural description.  The
// locales contain memory and a multi-core processor with homogeneous
// cores, and we ignore any affinity (NUMA effects) between the
// processor cores and the memory.  This architectural description is
// backward compatible with the architecture implicitly provided by
// releases 1.6 and preceding.
//
module LocaleModel {

  use LocaleModelHelpHSA;
  use LocaleModelHelpMem;

  class CPULocale : AbstractLocaleModel {
    const sid: chpl_sublocID_t;
    const name: string;

    proc chpl_id() return (parent:LocaleModel)._node_id; // top-level node id
    proc chpl_localeid() {
      return chpl_buildLocaleID((parent:LocaleModel)._node_id:chpl_nodeID_t,
                                sid);
    }
    proc chpl_name() return name;

    proc CPULocale() {
    }

    proc CPULocale(_sid, _parent) {
      sid = _sid;
      parent = _parent;
      name = parent.chpl_name() + "-CPU" + sid;
    }

    proc writeThis(f) {
      parent.writeThis(f);
      f <~> '.'+name;
    }

    proc getChildCount(): int { return 0; }
    iter getChildIndices() : int {
      halt("No children to iterate over.");
      yield -1;
    }
    proc addChild(loc:locale) {
      halt("Cannot add children to this locale type.");
    }
    proc getChild(idx:int) : locale { return nil; }
  }

  class GPULocale : AbstractLocaleModel {
    const sid: chpl_sublocID_t;
    const name: string;

    proc chpl_id() return (parent:LocaleModel)._node_id; // top-level node id
    proc chpl_localeid() {
      return chpl_buildLocaleID((parent:LocaleModel)._node_id:chpl_nodeID_t,
                                sid);
    }
    proc chpl_name() return name;

    proc GPULocale() {
    }
    proc ~GPULocale() {
     extern proc hsa_shutdown(): void ;
     hsa_shutdown();
    }


    proc GPULocale(_sid, _parent) {
      sid = _sid;
      parent = _parent;
      name = parent.chpl_name() + "-GPU" + sid;
    }

    proc writeThis(f) {
      parent.writeThis(f);
      f <~> '.'+name;
    }

    proc getChildCount(): int { return 0; }
    iter getChildIndices() : int {
      halt("No children to iterate over.");
      yield -1;
    }
    proc addChild(loc:locale) {
      halt("Cannot add children to this locale type.");
    }
    proc getChild(idx:int) : locale { return nil; }
  }

  const chpl_emptyLocaleSpace: domain(1) = {1..0};
  const chpl_emptyLocales: [chpl_emptyLocaleSpace] locale;

  //
  // The node model
  //
  class LocaleModel : AbstractLocaleModel {
    const _node_id : int;
    const local_name : string;

    const numSublocales: int;
    var GPU: GPULocale;
    var CPU: CPULocale;

    // This constructor must be invoked "on" the node
    // that it is intended to represent.  This trick is used
    // to establish the equivalence the "locale" field of the locale object
    // and the node ID portion of any wide pointer referring to it.
    proc LocaleModel() {
      if doneCreatingLocales {
        halt("Cannot create additional LocaleModel instances");
      }
      setup();
    }

    proc LocaleModel(parent_loc : locale) {
      if doneCreatingLocales {
        halt("Cannot create additional LocaleModel instances");
      }
      parent = parent_loc;
      setup();
    }

    proc chpl_id() return _node_id;     // top-level locale (node) number
    proc chpl_localeid() {
      return chpl_buildLocaleID(_node_id:chpl_nodeID_t, c_sublocid_any);
    }
    proc chpl_name() return local_name;


    proc writeThis(f) {
      // Most classes will define it like this:
      //      f <~> name;
      // but here it is defined thus for backward compatibility.
      f <~> new ioLiteral("LOCALE") <~> _node_id;
    }

    proc getChildSpace() {
      halt("error!");
      return {0..#numSublocales};
    }

    proc getChildCount() return numSublocales;

    iter getChildIndices() : int {
      for idx in {0..#numSublocales} do // chpl_emptyLocaleSpace do
        yield idx;
    }

    proc getChild(idx:int) : locale {
      if idx == 1
        then return GPU;
      else
        return CPU;
    }

    iter getChildren() : locale  {
      halt("in here");
      for idx in {0..#numSublocales} {
        if idx == 1
          then yield GPU;
        else
          yield CPU;
      }
    }

    proc getChildArray() {
      halt ("in get child Array");
      return chpl_emptyLocales;
    }


    //------------------------------------------------------------------------{
    //- Implementation (private)
    //-
    proc setup() {
      _node_id = chpl_nodeID: int;

      helpSetupLocaleHSA(this, local_name, numSublocales);
    }
    //------------------------------------------------------------------------}
 }

  //
  // An instance of this class is the default contents 'rootLocale'.
  //
  // In the current implementation a platform-specific architectural description
  // may overwrite this instance or any of its children to establish a more customized
  // representation of the system resources.
  //
  class RootLocale : AbstractRootLocale {

    const myLocaleSpace: domain(1) = {0..numLocales-1};
    var myLocales: [myLocaleSpace] locale;

    proc RootLocale() {
      parent = nil;
      nPUsPhysAcc = 0;
      nPUsPhysAll = 0;
      nPUsLogAcc = 0;
      nPUsLogAll = 0;
      maxTaskPar = 0;
    }

    // The setup() function must use chpl_initOnLocales() to iterate (in
    // parallel) over the locales to set up the LocaleModel object.
    // In addition, the initial 'here' must be set.
    proc setup() {
      helpSetupRootLocaleHSA(this);
    }

    // Has to be globally unique and not equal to a node ID.
    // We return numLocales for now, since we expect nodes to be
    // numbered less than this.
    // -1 is used in the abstract locale class to specify an invalid node ID.
    proc chpl_id() return numLocales;
    proc chpl_localeid() {
      return chpl_buildLocaleID(numLocales:chpl_nodeID_t, c_sublocid_none);
    }
    proc chpl_name() return local_name();
    proc local_name() return "rootLocale";

    proc writeThis(f) {
      f <~> name;
    }

    proc getChildCount() return this.myLocaleSpace.numIndices;

    proc getChildSpace() return this.myLocaleSpace;

    iter getChildIndices() : int {
      for idx in this.myLocaleSpace do
        yield idx;
    }

    proc getChild(idx:int) return this.myLocales[idx];

    iter getChildren() : locale  {
      for loc in this.myLocales do
        yield loc;
    }

    proc getDefaultLocaleSpace() const ref return this.myLocaleSpace;
    proc getDefaultLocaleArray() const ref return myLocales;

    proc localeIDtoLocale(id : chpl_localeID_t) {
      const node = chpl_nodeFromLocaleID(id);
      const subloc = chpl_sublocFromLocaleID(id);
      if (subloc == c_sublocid_none) || (subloc == c_sublocid_any) then
        return (myLocales[node:int]):locale;
      else
        return (myLocales[node:int].getChild(subloc:int)):locale;
    }
  }
}