// SCREENSAVER:
//  - should have running/swimming/flyinh crits like: 
//      fish, jellyfish, cold-blooded, mammals, birds, bugs
//  - should have shader-based background that shows:
//      fluid, air, surface of different kinds.
//  - simple way of mouse interaction

// DONE: simple crude fish with basic joint/bone based body and fins

// TODO:
// - complex pathing for cooler looking behaviour
// - bone based libs and inverse kinamatics
// - mammal type joints
// - rotation contraints (limit creature rotation so it cant break into itself)
// - smooth fill / outline (fish looks blocky, since its made of quads and straight lines)
// - creature AI.
// - spray food to attract fish
// - click on the fish to scary it
// - bugs as new type of the animal
// - fish linear trajectory laters its end while fish is traveling
// - INTERACTIVE DEBUG AND UPDATE/RENDER PIPELINE TOOLS 


// CONST

#define FPS 60

#define MAX_ENTITY_COUNT 64
#define MAX_JOINTS 32
#define MAX_FINS   (    MAX_JOINTS / 2    )
#define DEBUG_FONT_SIZE 14
#define DEBUG_FONT_PAD_SIZE 0

#define ZOOM_SPEED 0.1
#define SEGMENT_FIN_ROTATION_INFLUENCE 0.65
#define SPAWN_INTERVAL_IN_TICKS 10
#define COLOR_CHANGE_SPEED 4

// UTILITY
#define ENTITY (Entity)
#define VECTOR2 (Vector2)
#define RECTANGLE (Rectangle)
#define UNFOLD_V2(V) V.x, V.y
#define MIN(A,B) ((A) > (B) ? (B) : (A))
#define MAX(A,B) ((A) < (B) ? (B) : (A))
#define CMP(L,R) (memcmp(L,R,MIN(sizeof(*L),sizeof(*R))) == 0)
#define FMT_BOOL(E) (E) ? "true" : "false"
#define LITPTR_CAST(T,P) *((T*)P)

// libs / modules

#include <stdbool.h>
#include <string.h>

#ifdef RAYGUI_BACKEND
#   define RAYGUI_IMPLEMENTATION
#   include "../lib/raygui.h"
#endif

#include "../lib/rl-linux/include/raylib.h"
#include "../lib/rl-linux/include/raymath.h"

// hand written Memory Pool data structure
#include "../lib/pool.c"


// enums

typedef enum {
    PATH_LINE,
    PATH_BAZIER,
    PATH_COMPLEX
} PathType;

typedef enum {
    FIN_DEFAULT = 0,
    FIN_BACK,
    FIN_TAIL, 
    FIN_BLADE, // idk
} FinType;

// structs

typedef char byte;

typedef struct {
    Color   col;
    Vector2 pos;
    float   size;
    float   bonelen;

    Vector2 next_joint_v;
    Vector2 hips[2];
} Joint;

typedef struct {
    FinType type;
    union {
        struct {
            byte  joint_index;
            float angle;
            float length;
        } as_default;
        struct {
            byte begin, end;
        } as_backfin;
    };
    Color background;
    Color line;
} Fin;



// TODO: implement more interesting pathing system
// with waypoints and curved path lines.
typedef struct {
    PathType mode;
    float position;
    float waving_amplitude;
    float waving_period;
    union {
        struct {
            Vector2 start;
            Vector2 end;
        } as_line ;
    };
} Path;

typedef struct {
    bool    alive;
    bool    rainbow;
    size_t  self_index;
    float   linear_velocity;
    float   facing_angle;
    float   rainbow_hue;
    Vector2 vel;
    Vector2 pos;
    Vector2 accel;
    
    // slice for indexing sidefins
    size_t  fin_count;
    Fin     fins[MAX_FINS];

    // slice for indexing
    size_t  joint_count;
    Joint   body[MAX_JOINTS];


    Path    travel_path;

    // appearance
    float   eye_size;
    float   eye_center_offset;
    Color   line;
    Color   background;

} Entity;


typedef struct {
    bool pause;
    bool debug;
    bool rainbow_spawned;

    size_t  tick;
    Color   background;

    Vector2     window_dimensions;
    Camera2D    camera;
    Pool        entities;
} GlobalState;
