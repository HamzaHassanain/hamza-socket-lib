cmake_minimum_required(VERSION 3.10)
project(socket_lib)


# define a variable called debug, that I can acess in a c++ code, to enable debugging
set(DEBUG_MODE ON CACHE BOOL "Enable Debug Mode") 

if(DEBUG_MODE)
    add_definitions(-DDEBUG_MODE)
endif()


file(GLOB SRC_FILES src/*.cpp)


set(CMAKE_CXX_STANDARD 17)





# Function to read .env file and set variables
function(load_env_file env_file)
    if(EXISTS ${env_file})
        file(READ ${env_file} ENV_CONTENT)
        string(REPLACE "\n" ";" ENV_LINES ${ENV_CONTENT})
        
        foreach(line ${ENV_LINES})
            # Skip empty lines and comments
            if(line AND NOT line MATCHES "^#")
                string(FIND ${line} "=" eq_pos)
                if(eq_pos GREATER -1)
                    string(SUBSTRING ${line} 0 ${eq_pos} var_name)
                    math(EXPR val_start "${eq_pos} + 1")
                    string(SUBSTRING ${line} ${val_start} -1 var_value)
                    
                    # Remove any whitespace
                    string(STRIP ${var_name} var_name)
                    string(STRIP ${var_value} var_value)
                    
                    # Set the variable in parent scope
                    set(${var_name} ${var_value} PARENT_SCOPE)
                    message(STATUS "Loaded env variable: ${var_name}=${var_value}")
                endif()
            endif()
        endforeach()
    else()
        message(WARNING ".env file not found at ${env_file}")
    endif()
endfunction()

# Load environment variables from .env file
load_env_file(${CMAKE_SOURCE_DIR}/.env)



if(SOCKET_LOCAL_TEST AND SOCKET_LOCAL_TEST STREQUAL "1")
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -fsanitize=address -g -O0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()





if(SOCKET_LOCAL_TEST AND SOCKET_LOCAL_TEST STREQUAL "1")
    add_executable(  socket_lib app.cpp ${SRC_FILES}  )
else()
    add_library(socket_lib STATIC ${SRC_FILES})
endif()