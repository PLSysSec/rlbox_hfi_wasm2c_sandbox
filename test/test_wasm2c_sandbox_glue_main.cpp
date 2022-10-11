// #define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_RUNNER
#include "catch2/catch.hpp"

# ifdef HFI_EMULATION
#include "wasm-rt.h"
# endif

int main( int argc, char* argv[] ) {
# ifdef HFI_EMULATION
    wasm_rt_hfi_emulate_reserve_lower4();
# endif

  int result = Catch::Session().run( argc, argv );
  return result;
}
