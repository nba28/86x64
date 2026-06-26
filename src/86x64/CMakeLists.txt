add_library(wrapper STATIC wrapper.asm wrapper_setup.c)
add_library(interpose SHARED interpose.c)

# webnoop: a single-purpose DYLD-insert shim that suppresses legacy-WebKit
# (WebView) instantiation in the translated process (the JSC stack-bounds trap
# under our low-4GB stack). Built x86_64 (runs under Rosetta). See webnoop.m.
add_library(webnoop SHARED webnoop.m)
set_target_properties(webnoop PROPERTIES
  OSX_ARCHITECTURES x86_64
  COMPILE_OPTIONS "-fno-objc-arc")
target_link_libraries(webnoop "-framework Foundation" "-framework AppKit")

# acctnoop: a single-purpose DYLD-insert shim that neutralizes iPhoto's DEAD
# MobileMe / unknown-account startup discovery (IPAccountConfigurationManager
# findUnknownAccounts / findUnknownSystemMobileMeAccount), which crashes ~50% of
# launches under translation. Built x86_64 (runs under Rosetta). See acctnoop.m.
add_library(acctnoop SHARED acctnoop.m)
set_target_properties(acctnoop PROPERTIES
  OSX_ARCHITECTURES x86_64
  COMPILE_OPTIONS "-fno-objc-arc")
target_link_libraries(acctnoop "-framework Foundation")

install(TARGETS wrapper interpose webnoop acctnoop
  DESTINATION opt/86x64
  )

install(PROGRAMS 86x64 86x64.sh static-interpose.sh abiconv.sh
  DESTINATION opt/86x64
  )

install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"${CMAKE_INSTALL_PREFIX}/opt/86x64/86x64.sh\" \"${CMAKE_INSTALL_PREFIX}/bin/86x64\")")
