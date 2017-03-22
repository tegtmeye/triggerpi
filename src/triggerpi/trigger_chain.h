#ifndef TRIGGERPI_TRIGGER_CHAIN_H
#define TRIGGERPI_TRIGGER_CHAIN_H

#include "expansion_board.h"

#include "builtin_trigger.h"

#include <memory>
#include <map>
#include <string>


typedef std::pair<std::shared_ptr<expansion_board>,
  std::shared_ptr<expansion_board> > trigger_production;

/*
  Build the given trigger production and return a vector of UNLINKED
  trigger_production objects. Each element of the return vector is either
  an existing value in \production_map or a production representing a NEW
  expansion_board in which case the element trigger_production has the same
  value for the first and second (ie it is a linked list of size 1)

  If each triggerspec is:
    - Not a valid production label then throw an error
    - A builtin trigger spec
      - create and install a new builtin trigger and use it. Do not assign
        a production label to it

  If the first triggerspec is:
    - An existing trigger production
      - check to see if the production tail is configured
        as some kind of source, if not then throw an error
      - otherwise use the tail as the next production source

  If the nth triggerspec is:
    - Already has a source, then throw an error
    - An existing trigger production
      - Ensure the n-1th trigger production tail has a compatible
        trigger_source as the nth trigger production head trigger_sink


    Labels that start with '__' are reserved

    Productions can be anonymous which means that they cannot be referred to
    later

    --trigger=2s[100ms:100ms]2m --trigger=...

    To reuse a builtin trigger use

    --trigger=2s[100ms:100ms]2m@start_label --trigger="start_label|foo|..."
      --trigger="start_label|bar|..."

    It is also possible to make aliases

    --trigger=2s[100ms:100ms]2m@start_label --trigger=start_label@start_alias
*/
template<typename CharT>
std::vector<trigger_production>
make_trigger_chain(const std::basic_string<CharT> &trigger_spec,
  const std::map<std::basic_string<CharT>,trigger_production> &production_map)
{
  typedef std::basic_string<CharT> string_type;

  std::vector<trigger_production> result;

  /*
    First triggerspec needs to be handled differently as it is allowed to
    be a sink in an existing chain. Which means that this production is
    a fork in some existing trigger production.
  */
  auto prev = trigger_spec.begin();
  auto loc_iter = std::find(prev,trigger_spec.end(),'|');

  string_type triggerref_str(prev,loc_iter);
  auto map_iter = production_map.find(triggerref_str);
  if(map_iter == production_map.end()) {
    std::shared_ptr<expansion_board> builtin =
      make_builtin_trigger(triggerref_str);
    result.emplace_back(std::make_pair(builtin,builtin));
  }
  else {
    // make sure it doen't reference an anonymous production
    result.emplace_back(map_iter->second);
  }

  if(loc_iter == trigger_spec.end())
    return result;

  prev = ++loc_iter; // eat the '|'
  while(prev != trigger_spec.end()) {
    std::shared_ptr<expansion_board> cur_source = result.back().second;

    auto loc_iter = std::find(prev,trigger_spec.end(),'|');

    triggerref_str.assign(prev,loc_iter);
    map_iter = production_map.find(triggerref_str);
    if(map_iter == production_map.end()) {
      std::shared_ptr<expansion_board> builtin =
        make_builtin_trigger(triggerref_str);
      result.emplace_back(std::make_pair(builtin,builtin));
    }
    else {
      // make sure it doen't reference an anonymous production
      result.emplace_back(map_iter->second);
    }

    const std::shared_ptr<expansion_board> &cur_sink = result.back().first;

    if(cur_sink->is_trigger_sink()) {
      std::stringstream err;
      err << "trigger reference '" << triggerref_str
        << "' is already configured as a trigger sink";
      throw std::runtime_error(err.str());
    }

    if((cur_source->trigger_source_type() & cur_sink->trigger_sink_type()) ==
      trigger_type::none)
    {
      std::stringstream err;
      err << "trigger reference '" << triggerref_str
        << "' with trigger sink type '"
        << cur_sink->trigger_sink_type() << "' is incompatible with the "
          "given source expansion '" << cur_source->system_description()
        << "' with trigger source type '" << cur_source->trigger_source_type()
        << "'";
      throw std::runtime_error(err.str());
    }

    if(loc_iter != trigger_spec.end())
      ++loc_iter;

    prev = loc_iter;
  }

  return result;
}

#endif
