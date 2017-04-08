/*
  Data source that just reproduces the number given to it
*/

#ifndef TRIGGERPI_BUILTIN_CONST_DATASOURCE_H
#define TRIGGERPI_BUILTIN_CONST_DATASOURCE_H

#include "expansion_board.h"

#include <string>
#include <map>
#include <thread>

class builtin_const_datasource :public expansion_board {
  public:
    builtin_const_datasource(int i)
      :expansion_board(trigger_type_t::multi_sink,
        data_flow_t::source|data_flow_t::optional_source), _val(i) {}

      virtual void initialize(void) {
        std::map<std::string,std::string> config;

        configure_data_source_blocks(1024*sizeof(int),alignof(int),128,config);

      }

      virtual void run(void) {
        while(wait_on_trigger_start()) {
          data_block_ptr data = allocate_data_block();

          if(!data.get()) {
            std::this_thread::yield();
            continue;
          }

          int *block = static_cast<int*>(data.get());
          int count = 1;
          while(is_triggered() && count<1024)
            block[count++] = _val;

          block[0] = count;
          push_data_block(data); // report full
        }
      }

      virtual std::string system_identifier(void) const {
        return std::to_string(_val);
      }

      virtual std::string system_description(void) const {
        return std::string(system_prefix)+" "+system_identifier();
      }

  private:
    static constexpr const char * system_prefix
      = "const value datasource";

    int _val;

};

class test_datasource :public expansion_board {
  public:
    test_datasource(void)
      :expansion_board(trigger_type_t::none,data_flow_t::sink) {}

      virtual void initialize(void) {
      }

      virtual void data_sink_config(
        const std::map<std::string,std::string> &)
      {
        configure_data_sink_queue(128);
      }

      virtual void run(void) {
        std::size_t nblocks = 0;

        std::size_t res = 0;

        volatile bool flag = true;
        while(flag) {

          while(!data_block_available())
            std::this_thread::yield();

          data_block_ptr raw_block = pop_data_block();
          ++nblocks;

          int *block = static_cast<int*>(raw_block.get());
          if(nblocks % 1000000 == 0) {
            std::cout << "reading block: " << nblocks << " of size: "
              << block[0] << "\n";
          }

          for(std::size_t i=1; i<block[0]; ++i)
            res += block[i];
        }
        std::cout << res << "\n";
      }

      virtual std::string system_identifier(void) const {
        return "test";
      }

      virtual std::string system_description(void) const {
        return std::string(system_prefix)+" "+system_identifier();
      }

  private:
    static constexpr const char * system_prefix
      = "datasink";

};




template<typename CharT>
std::shared_ptr<expansion_board>
make_builtin_dataflow(const std::basic_string<CharT> &raw_str)
{
  if(raw_str == "test")
    return std::shared_ptr<expansion_board>(new test_datasource());

  int const_val = std::stoul(raw_str);

  return std::shared_ptr<expansion_board>(
    new builtin_const_datasource(const_val));

}
#endif
