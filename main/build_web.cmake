# Builds the web app in web/ (run as a script with cmake -P from the "web_app" target).
# Expects NPM (path to npm) and WEB_DIR to be defined.

if(NOT EXISTS "${WEB_DIR}/node_modules")
    message(STATUS "Installing web app dependencies")
    execute_process(COMMAND "${NPM}" install
                    WORKING_DIRECTORY "${WEB_DIR}"
                    COMMAND_ERROR_IS_FATAL ANY)
endif()

message(STATUS "Building web app")
execute_process(COMMAND "${NPM}" run build
                WORKING_DIRECTORY "${WEB_DIR}"
                COMMAND_ERROR_IS_FATAL ANY)
