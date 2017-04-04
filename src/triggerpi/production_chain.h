#ifndef TRIGGERPI_PRODUCTION_CHAIN_H
#define TRIGGERPI_PRODUCTION_CHAIN_H

#include "expansion_board.h"

#include <memory>
#include <map>
#include <string>
#include <functional>

typedef std::pair<std::shared_ptr<expansion_board>,
  std::shared_ptr<expansion_board> > production_chain;

/*
  Build the given production and return a vector of UNLINKED
  production_chain objects. Each element of the return vector is either
  an existing value in \production_map or a production representing a NEW
  expansion_board in which case the element production_chain has the same
  value for the first and second (ie it is a linked list of size 1)

  If each production_spec is:
    - Not a valid production label then throw an error
    - A builtin expansion specification
      - create and install a new builtin expansion and use it. Do not assign
        a production label to it

  If the first production_spec is:
    - An existing production
      - check to see if the production tail is configured
        as some kind of source, if not then throw an error
      - otherwise use the tail as the next production source

  If the nth production_spec is:
    - Already has a source, then throw an error
    - An existing production
      - Ensure the n-1th production tail has a compatible
        source as the nth data production head sink


    Productions can be anonymous which means that they cannot be referred to
    later

    --dataflow=0 --dataflow=...

    To reuse a builtin dataobj use

    --dataflow=0@zeros --dataflow="zeros|foo|..." --dataflow="zeros|bar|..."

    It is also possible to make aliases

    --dataflow=0@zeros --dataflow=zeros@zeros_alias
*/
template<typename CharT>
std::vector<production_chain>
make_production_chain(const std::basic_string<CharT> &prod_spec,
  const std::map<std::basic_string<CharT>,production_chain> &production_map,
  std::shared_ptr<expansion_board>(*make_builtin)(
    const std::basic_string<CharT> &))
{
  typedef std::basic_string<CharT> string_type;

  std::vector<production_chain> result;

  /*
    First production spec needs to be handled differently as it is allowed to
    be a sink in an existing chain. Which means that this production is
    a fork in some existing data production.
  */
  auto prev = prod_spec.begin();
  auto loc_iter = std::find(prev,prod_spec.end(),'|');

  string_type expansionref_str(prev,loc_iter);
  auto map_iter = production_map.find(expansionref_str);
  if(map_iter == production_map.end()) {
    std::shared_ptr<expansion_board> builtin =
      make_builtin(expansionref_str);
    result.emplace_back(std::make_pair(builtin,builtin));
  }
  else {
    // make sure it doen't reference an anonymous production
    result.emplace_back(map_iter->second);
  }

  if(loc_iter == prod_spec.end())
    return result;

  prev = ++loc_iter; // eat the '|'
  while(prev != prod_spec.end()) {
    std::shared_ptr<expansion_board> cur_source = result.back().second;

    auto loc_iter = std::find(prev,prod_spec.end(),'|');

    expansionref_str.assign(prev,loc_iter);
    map_iter = production_map.find(expansionref_str);
    if(map_iter == production_map.end()) {
      std::shared_ptr<expansion_board> builtin =
        make_builtin(expansionref_str);
      result.emplace_back(std::make_pair(builtin,builtin));
    }
    else {
      // make sure it doen't reference an anonymous production
      result.emplace_back(map_iter->second);
    }

    if(loc_iter != prod_spec.end())
      ++loc_iter;

    prev = loc_iter;
  }

  return result;
}

#endif
