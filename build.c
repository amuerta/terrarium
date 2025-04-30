#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define NOB_IMPLEMENTATION
#include "./lib/nob.h"

#define EXEC_NAME           "screensaver"
#define CMD_RUN             "run"
#define CMD_COMPILE         "compile"
#define PLATFORM_FROM_LINUX_TO_WINDOWS \
                            "linux-to-windows"
#define PLATFORM_LINUX      "linux"
#define MODE_DEBUG          "debug"
#define MODE_RELEASE        "release"

int build(char* mode, char* platform) {
    Nob_Cmd cmd = {0};
    if (strcmp(platform,PLATFORM_LINUX)==0) {
        if (strcmp(mode,MODE_RELEASE)==0)
            nob_cmd_append(&cmd,
                    "cc", 
                    "-Wall", 
                    "-Wextra", 
                    "-I./lib/rl-linux/include/", 
                    "-L./lib/rl-linux/lib/",
                    "-lraylib",
                    "-lX11",
                    "-lGL",
                    "-lm",
                    "-o", 
                    EXEC_NAME, 
                    "./src/main.c");
        else if (strcmp(mode,MODE_DEBUG)==0)
            nob_cmd_append(&cmd,
                    "cc", 
                    "-Wall", 
                    "-Wextra", 
                    "-fsanitize=address",
                    "-I./lib/rl-linux/include/", 
                    "-L./lib/rl-linux/lib/",
                    "-lraylib",
                    "-lX11",
                    "-lGL",
                    "-lm",
                    "-o", 
                    EXEC_NAME, 
                    "./src/main.c");
    }
    else if (strcmp(platform,PLATFORM_FROM_LINUX_TO_WINDOWS)==0) {
        if (strcmp(mode,MODE_RELEASE)==0)
            nob_cmd_append(&cmd, 
                    "x86_64-w64-mingw32-gcc", 
                    "-o", EXEC_NAME".exe", 
                    "./src/main.c",
                    "-I./lib/rl-windows/include/", 
                    "-L./lib/rl-windows/lib/",
                    "-lraylib",
                    "-lwinmm",
                    "-lgdi32");
        else
            nob_cmd_append(&cmd, 
                    "x86_64-w64-mingw32-gcc", 
                    "-o", EXEC_NAME".exe", "main.c",
                    "-Wall", "-Wextra", 
                    "-I./lib/rl-window/include/", 
                    "-L./lib/rl-window/lib/",
                    "-lraylib",
                    "-lwinmm",
                    "-lgdi32");
    }
    else {
        nob_log(NOB_ERROR,  "Unsupported platform: %s",platform);
        nob_log(NOB_INFO,   
                "Avilable platforms:\n"
                " - windows"
                " - linux"
        );
        return -1;
    }

    return nob_cmd_run_sync(cmd);
}

int run(void) {
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "./"EXEC_NAME);
    return nob_cmd_run_sync(cmd);
}

bool has_flag(const char* flag,int argc, char** argv) {
    for(int i = 1; i < argc; i++)
        if (strcmp(flag,argv[i])==0) return true;
    return false;
}


int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    bool debug             = has_flag(MODE_DEBUG,argc,argv);
    bool target_windows    = has_flag(PLATFORM_FROM_LINUX_TO_WINDOWS,argc,argv);
    bool target_linux      = has_flag(PLATFORM_LINUX,argc,argv);

    if(target_windows && target_linux) {
        nob_log(NOB_ERROR,"Pick one compilation target.");
        return -1;
    } else if (target_windows) {
        build(debug ? MODE_DEBUG:MODE_RELEASE, PLATFORM_FROM_LINUX_TO_WINDOWS);
    } else if (target_linux) {
        build(debug ? MODE_DEBUG:MODE_RELEASE, PLATFORM_LINUX);
    } else {
        nob_log(NOB_INFO,
                "USAGE: "       "\n"
                "\t\t%s !TARGET ?MODE"         "\n"
                "\n"
                " + "   "FLAGS:\n"
                "\t"    "TARGET(s): [ linux, linux_to_windows ]" "\n"
                "\t"    "MODE: ( debug, release )" "\n"
                " + "   "IMPORTANCE:\n"
                "\t"    "? - optional field" "\n"
                "\t"    "! - forced  field" "\n"
                ,
                
                argv[0]
        );
        return -1;
    }

 //   if(!build("debug","linux"))
   //     return -1;
    if(has_flag(CMD_RUN,argc,argv) && !run())
        return -1;
}
