find_package(XMLRPC REQUIRED c++2 client)

add_library(client STATIC
  src/client.cpp
)

target_include_directories(client PUBLIC "include/")
target_include_directories(client PRIVATE "${XMLRPC_INCLUDE_DIRS}")
target_link_libraries(client PRIVATE "${XMLRPC_LIBRARIES}")
target_compile_features(client PUBLIC cxx_std_17)

add_executable(clientDemo src/demo_main.cpp)
target_link_libraries(clientDemo PUBLIC client)
