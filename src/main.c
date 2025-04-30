#include "defs.h"

void draw_joint_debug(Joint j, int index) {
    Color col = (index == 0) ? RED : Fade(RAYWHITE,0.5);
    DrawCircleV(j.pos,j.size,col);
}

Vector2 get_joints_bone(Joint* l, Joint* r, bool tail) {
    if (tail) 
        return Vector2Subtract(l->pos,r->pos);
    else 
        return Vector2Subtract(r->pos,l->pos);
}

Vector2 Vector2Normal(Vector2 v) {
    return (Vector2) {-v.y, v.x};
}



void update_joint_hips(Entity* entity) 
    //Vector2 next_direction = Vector2Subtract(next->pos,spine->pos);
{
    Joint* spine = entity->body;
    for(size_t i = 0; i < entity->joint_count; i++) {
        Joint 
            *previous   = spine + i - 1,
            *current    = spine + i,
            *next       = spine + (i+1);
        
        Vector2 next_direction =  {0};
        //next_direction = get_joints_bone(next, current, i < entity->joints - 1);

        if (i < entity->joint_count - 1) 
            next_direction = Vector2Subtract(next->pos,current->pos);
        else 
            next_direction = Vector2Subtract(current->pos,previous->pos);
        
        next_direction = Vector2Scale(Vector2Normalize(next_direction),current->size);

        Vector2 
            hip1 = Vector2Rotate(next_direction,90*DEG2RAD), 
            hip2 = Vector2Rotate(next_direction,-90*DEG2RAD);

        current->hips[0] = Vector2Add(current->pos,hip1);
        current->hips[1] = Vector2Add(current->pos,hip2);
        current->next_joint_v = next_direction;



        //DrawCircleV(current->hips[0],5,RED);
        //DrawCircleV(current->hips[1],5,RED);
    }
}

float get_path_length(Path* p) {
    switch(p->mode) {
        case PATH_LINE: 
            {
                return Vector2Length(Vector2Subtract(p->as_line.end,p->as_line.start));
            }
        default: return 0;
    }
}


void update_entity_spawn_origin(Entity* entity) {
    Joint* spine = entity->body;
    for(size_t i = 0; i < entity->joint_count; i++) {
        Joint 
            *current    = spine + i;
        current->pos = entity->pos;
    }
}

void update_joint_constrains(Entity* entity) {
    Joint* spine = entity->body;
    
    for(size_t i = 0; i < entity->joint_count; i++) {
        Joint 
            *current    = spine + i,
            *next       = spine + (i+1);
        
        // if next is valid entity compute constrain
        if (i < entity->joint_count - 1) {
            Vector2 next_direction = Vector2Subtract(next->pos,current->pos);
            next_direction = Vector2Scale(Vector2Normalize(next_direction),current->bonelen);
            next->pos = Vector2Add(current->pos,next_direction);
            //DrawLineEx(spine->pos, next->pos, 10, RED);
        }
    }
}


Vector2 update_path(Entity* p, bool debug);

void update_position(Entity* ent) {
#if 0
    Vector2 UP = {0,-1};
    Vector2 direction = Vector2Rotate(UP,ent->facing_angle);
    Vector2 direction_velocity = Vector2Scale(direction, ent->vel);
    ent->pos = Vector2Add(ent->pos,direction_velocity);
    
    //if (IsKeyDown(KEY_W))
    ent->body[0].pos = ent->pos;
#endif

    Vector2 future = Vector2Add(ent->pos,ent->vel);

    ent->pos = future;
    ent->body[0].pos = future;
}

void despawn_marked_entity(void* e, void* p) {
    Pool*   pool    = p;
    Entity* ent     = e;

    if (!ent->alive) {
        pool_release(pool,ent->self_index);
        memset(ent,0,sizeof(*ent));
    }

}

void update_entity(void* e, void* out) {
    bool    debug = LITPTR_CAST(bool,out);
    bool*   have_rainbow_fish = out + sizeof(bool);
    Entity* ent = e;

    Path* path = &(ent->travel_path);

    if (ent->rainbow) {
        *have_rainbow_fish  = true;
        ent->rainbow_hue  += COLOR_CHANGE_SPEED;
        ent->background    = ColorFromHSV(ent->rainbow_hue,0.5,0.5);
    }

        //if (!state.pause)

    bool ongoing_path = path->position < get_path_length(path) - ent->linear_velocity*2;
    bool within_path = path->position > ent->linear_velocity * 2 && ongoing_path;


    if (!within_path) {
        // in case where we dont despawn the ent, we reset its pos/state.
        update_entity_spawn_origin(ent);
        // currently we mark it do be despawned
        if (!ongoing_path) // finished its path
            ent->alive = false;
    } 

        
    update_position(ent);
    ent->vel = update_path(ent, debug);
    


    //if (within_path) {
    update_joint_hips(ent);
    update_joint_constrains(ent);
    //}
}

Entity get_default_entity_fish(void);
void append_entity(GlobalState*, Entity);
Entity get_entity_fish_with_path(Vector2* line);

Entity get_entity_fish_with_path(Vector2* line) {
    Entity e = get_default_entity_fish();
    e.travel_path.as_line.start = line[0];
    e.travel_path.as_line.end   = line[1];
    return e;
}

Rectangle RectangleScale(Rectangle r, float multiplier) {
    return RECTANGLE {
        r.x - (r.width * multiplier - r.width )/2,
        r.y - (r.height* multiplier - r.height)/2,
        r.width * multiplier,
        r.height * multiplier
    };
}

void update_fish_spawn(GlobalState* gs) {

    Pool* p = &(gs->entities);

    Vector2 win = {
        gs->window_dimensions.x,
        gs->window_dimensions.y,
    };
    Vector2 win_world = {
        win.x * 1/gs->camera.zoom,
        win.y * 1/gs->camera.zoom,
    };

    // TODO: implement smart system for handling spawning and despawning 
    // with screen begin able to zoom-ed out.
    Rectangle screen_to_world = {
        .x = -win.x /2,
        .y = -win.y /2,
        .width = win.x,
        .height = win.y,
    };

    // create points that are within spawn_zone but 
    // dont collide with screen rec.
    // make sure they cross diagonal.
    
    Rectangle world_spawn_zone = {
        screen_to_world.x * 2,
        screen_to_world.y * 2,
        screen_to_world.width * 2,
        screen_to_world.height * 2,
    };

    Vector2 lim_h = {
        world_spawn_zone.x,
        world_spawn_zone.x + world_spawn_zone.width
    };

    Vector2 lim_w = {
        world_spawn_zone.y,
        world_spawn_zone.y + world_spawn_zone.height
    };

    // line
    Vector2 diagonal[2] = {
        {
            screen_to_world.x,
            screen_to_world.y
        },
        {
            screen_to_world.x + screen_to_world.width,
            screen_to_world.y + screen_to_world.height
        },
    };

    Vector2 path [2]; // begin, end

    for(int i = 0; i < 2; i++)
        path[i] = VECTOR2 {
            GetRandomValue(lim_h.x,lim_h.y),
            GetRandomValue(lim_w.x,lim_w.y),
        };


    Vector2 unused_colpoint;


    Rectangle screen_overhead = RectangleScale(screen_to_world,1.5);

    if (gs->debug) {
        DrawLineEx(diagonal[0],diagonal[1],2,RED);
        DrawRectangleLinesEx(screen_to_world,2,RED);
        DrawRectangleRec(world_spawn_zone,Fade(YELLOW,0.1));
        DrawRectangleLinesEx(screen_overhead,2,ORANGE);
    }

    bool 
        intersects_diagonal = 
            CheckCollisionLines(diagonal[0],diagonal[1],path[0],path[1], &unused_colpoint),
        on_screen = (
            CheckCollisionPointRec(path[0],screen_overhead) ||
            CheckCollisionPointRec(path[1],screen_overhead) ),
        spawn_tick = 
            gs->tick % SPAWN_INTERVAL_IN_TICKS == 0,
        fits_pool = p->count + 1 < p->capacity;

    ;

    if (!on_screen && intersects_diagonal && fits_pool && spawn_tick) {
        Entity fish = get_entity_fish_with_path(path);
        // TODO: make it not hardcodded and magic number-ish
        fish.linear_velocity = GetRandomValue(1.5,3.5);
        fish.rainbow = (GetRandomValue(0,100) == 42);
        switch(GetRandomValue(0,20)) {
            case 1:
                fish.fin_count = 4; break;
            case 2:
                fish.fin_count = 3; break;
            default: ;
        }

        fish.travel_path.waving_amplitude = GetRandomValue(25,45);
        fish.travel_path.waving_period = GetRandomValue(250,300);

        append_entity(gs, fish);
    }
}

void update(GlobalState* gs) {
    if (gs->pause) return;

    bool rainbow_spawned = false;
    bool param[2] = { gs->debug, rainbow_spawned};

    Pool* ents = &(gs->entities);
    pool_iter(ents, update_entity, param);
    
    
    update_fish_spawn(gs);

    // despawn mechanism
    pool_iter(ents, despawn_marked_entity, ents);
    gs->rainbow_spawned = param[1];
    gs->tick++;
}

typedef struct {
    Vector2 vertices[4];
} Quad;

void DrawQuad(Quad q, Color c) {
    Vector2* verts = q.vertices;
    DrawTriangle(verts[1],verts[0],verts[2],c);
    DrawTriangle(verts[1],verts[2],verts[3],c);
}

void draw_entity_front_skin(Joint* previous, Joint* current, Vector2 hip1, Vector2 hip2) {
    Vector2 diff = Vector2Subtract(previous->pos,current->pos);
    Vector2 reverse = Vector2Scale(Vector2Normalize(diff),-current->size);
    Vector2 rotations[2] = {
        Vector2Add(current->pos,Vector2Rotate(reverse,-45 * DEG2RAD)),
        Vector2Add(current->pos,Vector2Rotate(reverse, 45 * DEG2RAD)),
    };
    Vector2 face = Vector2Add(current->pos,reverse);


    DrawTriangle(hip1,face,hip2,current->col);
    DrawTriangle(hip1,face,rotations[0],current->col);
    DrawTriangle(hip2,rotations[1],face,current->col);

    DrawTriangle(hip1,rotations[1],face,current->col);
    DrawTriangle(rotations[0],hip2,face,current->col);

    DrawLineEx(hip1,rotations[1],2,WHITE);
    DrawLineEx(rotations[1],face,2,WHITE);
    DrawLineEx(face,rotations[0],2,WHITE);
    DrawLineEx(rotations[0],hip2,2,WHITE);
    /*
       DrawLineV(face, current->pos,RED);
       DrawLineV(face, rotations[0],RED);
       DrawLineV(face, rotations[1],RED);
    */
}

void draw_entity_hip_skin(Vector2* h1, Vector2* h2, Color c) {
    Quad segment = {{ h1[0],h1[1],h2[0],h2[1] }};
    DrawQuad(segment, c);
    DrawLineEx(h1[0],h2[0],2,WHITE);
    DrawLineEx(h1[1],h2[1],2,WHITE);
    /*
       DrawLineBezier(current->hips[0],next->hips[0],2,WHITE);
       DrawLineBezier(current->hips[1],next->hips[1],2,WHITE);
    */
}

void draw_entity_eyes(Entity* e) {
    float size = e->eye_size;
    float offset = e->eye_center_offset;
    Joint* previous = e->body + 1;
    Joint* current = e->body + 0;
    Vector2 diff = Vector2Subtract(previous->pos,current->pos);
    Vector2 reverse = Vector2Scale(Vector2Normalize(diff),-(current->size/(1+offset) ) );
    Vector2 eyes[2] = {
        Vector2Add(current->pos,Vector2Rotate(reverse,-90 * DEG2RAD)),
        Vector2Add(current->pos,Vector2Rotate(reverse, 90 * DEG2RAD)),
    };
    DrawCircleV(eyes[0],size,RAYWHITE);
    DrawCircleV(eyes[1],size,RAYWHITE);
}

void draw_entity_fins(Joint* b, Joint* e, float fin_len, float fin_angle) {
    Vector2 l = 
        Vector2Scale(Vector2Rotate(Vector2Normalize(b->next_joint_v),DEG2RAD* fin_angle),fin_len);
    Vector2 r = 
        Vector2Scale(Vector2Rotate(Vector2Normalize(b->next_joint_v),DEG2RAD*-fin_angle),fin_len);


    DrawTriangle(b->hips[0],Vector2Add(b->pos,l),e->hips[0],b->col);
    DrawTriangle(b->hips[1],e->hips[1],Vector2Add(b->pos,r),b->col);
    //DrawTriangle(b->hips[0],l,e->hips[0],RED);

    DrawLineV(e->hips[0],Vector2Add(b->pos,l),RAYWHITE);
    DrawLineV(e->hips[1],Vector2Add(b->pos,r),RAYWHITE);
    DrawLineEx(b->hips[0],Vector2Add(b->pos,l),2,RAYWHITE);
    DrawLineEx(b->hips[1],Vector2Add(b->pos,r),2,RAYWHITE);
}



void draw_entity_fins_at_segment(Joint* b, Joint* e, void* slice, byte index, float segment_angle) {
    size_t  size        = LITPTR_CAST(size_t,slice);
    Fin*    fin_data    = slice + sizeof(size_t);

    float segment_diff_angle = SEGMENT_FIN_ROTATION_INFLUENCE * segment_angle * RAD2DEG;

    for(size_t i = 0; i < size; i++) {
        Fin f = fin_data[i];
        if (f.type == FIN_DEFAULT && index == f.as_default.joint_index) {
            float angle = f.as_default.angle;
            float length = f.as_default.length;
            draw_entity_fins(b, e, length, angle - segment_diff_angle);
        } 
    }
}

// TODO: improve safety and add features
void draw_entity_back_fins(Entity* entity,float segment_angle) {

    Joint *joint_b, *joint_e, *middle;
    
    // just-in-case fault safety
    for(size_t i = 0; i < entity->fin_count; i++) {
        Fin f = entity->fins[i];
        if (f.type == FIN_BACK) {
            size_t min = MIN(f.as_backfin.begin, f.as_backfin.end);
            size_t max = MAX(f.as_backfin.begin, f.as_backfin.end);
         
            if (max > entity->joint_count) break; // fault prevention 
            for(size_t i = 0; i < entity->joint_count; i++) {
                if(i == min) {
                    joint_b = entity->body + i;
                    break;
                }
            }
            if (!joint_b) continue; // fault prevention

            size_t length = (max-min);
            joint_e = joint_b + length;
            
            if (length > 1) {
                middle  = joint_b + (length/2);
            } else 
                middle  = joint_e;

            bool last = i < entity->joint_count - 1;

            Vector2 bone = get_joints_bone(joint_b,middle,last); // TODO: check
            Vector2 bone_normal = Vector2Scale(Vector2Normalize(Vector2Normal(bone)), -segment_angle * RAD2DEG * 0.3);
            Vector2 bone_offset = Vector2Add(middle->pos, bone_normal);

            for(size_t i = 0; i < length; i++) {

                Joint* current = joint_b + i;
                Joint* next = joint_b + i + 1;

                DrawLineEx(current->pos, next->pos,2,WHITE);
            }

            //DrawLineEx(middle->pos,bone_offset,2,WHITE);
            DrawLineEx(joint_e->pos,bone_offset,2,WHITE);
            DrawLineEx(joint_b->pos,bone_offset,2,WHITE);
        }
    }
}

void draw_entity(void* e, void* is_debug) {
    bool debug = LITPTR_CAST(bool, is_debug);
    Entity* entity = e;
    Path* path = &(entity->travel_path);
    Joint* spine = entity->body;
    float spine_curvature = 0;
    float segment_angle = 0;


    bool within_path = path->position > entity->linear_velocity*2 
        && path->position < get_path_length(path) - entity->linear_velocity*2;

    if(!within_path)
        return;

    for(size_t i = 0; i < entity->joint_count; i++) {
        Joint 
            *previous   = spine + i - 1,
            *current    = spine + i,
            *next       = spine + (i+1);
        
        void* fin_slice = &(entity->fin_count);
        Vector2 hip1, hip2;
       
        if (entity->rainbow) {
            float hue = entity->rainbow_hue + i * 2 * COLOR_CHANGE_SPEED;
            current->col = ColorFromHSV(hue,0.3,0.3);
        }


        // if next is valid entity compute constrain
        if (i < entity->joint_count - 1) {
            hip1 = current->hips[1];
            hip2 = current->hips[0];
            segment_angle = 0;

            bool last = i < entity->joint_count - 1;
            
            if (i  > 0) {
                Vector2 bone_back = get_joints_bone(current, next, last);
                Vector2 bone_front = get_joints_bone(previous, current, last);
                segment_angle  = Vector2Angle(bone_front,bone_back);
                spine_curvature += segment_angle;  
                if (debug) {
                    DrawLineEx(Vector2Add(Vector2Scale(bone_front,-1),current->pos),current->pos, 1, ORANGE);
                    DrawLineEx(Vector2Add(Vector2Scale(bone_back,-1),current->pos),current->pos, 1, YELLOW);
                }
            }

            draw_entity_fins_at_segment(current,next,fin_slice,i,segment_angle);

            if (i == 0) draw_entity_front_skin(next,current,hip1,hip2);
            draw_entity_hip_skin(current->hips, next->hips, current->col);



            if (debug) DrawLineV(current->pos, next->pos,GREEN);
        } else {
            hip1 = current->hips[0];
            hip2 = current->hips[1];

            //if ()
            draw_entity_front_skin(previous,current,hip1,hip2);
        }

        if (debug) draw_joint_debug(*current,i);
    }
    //printf("body_curavture_deg = %f\n",segment_angle * RAD2DEG);
    
    draw_entity_back_fins(entity,segment_angle);
    draw_entity_eyes(entity);
}

#define DRAW_TEXT_FMT(O,I,...)  DrawText(\
            TextFormat(__VA_ARGS__),\
            O.x,\
            O.y + (I)*DEBUG_FONT_SIZE + (I)*DEBUG_FONT_PAD_SIZE,\
            DEBUG_FONT_SIZE,RAYWHITE    );

void hud_statuspanel(GlobalState* gs) {
    size_t ent_cap = gs->entities.capacity;
    size_t ent_count = gs->entities.count;
    size_t ent_max_count = gs->entities.max_size;
    int c = 0;
    Vector2 origin = {10,10};

    DRAW_TEXT_FMT(origin,c++, "Runtime info:");
    DRAW_TEXT_FMT(origin,c++, "FPS/UPS, frametime: %i f/s, %f s", GetFPS(), GetFrameTime());
    DRAW_TEXT_FMT(origin,c++, "Tick: %lu", gs->tick);
    DRAW_TEXT_FMT(origin,c++, "Entity info:");
    DRAW_TEXT_FMT(origin,c++, "rainbow_spanwed: %s", gs->rainbow_spawned?  "true":"false");
    DRAW_TEXT_FMT(origin,c++, "entity_capacity: %lu", ent_cap);
    DRAW_TEXT_FMT(origin,c++, "entity_count: %lu", ent_count);
    DRAW_TEXT_FMT(origin,c++, "entity_max_count: %lu", ent_max_count);
}

void draw(GlobalState* gs) {
    Pool* ents = &(gs->entities);
    
    ClearBackground(gs->background);
    pool_iter(ents, draw_entity,&gs->debug);
}

void append_entity(GlobalState* gs, Entity e) {
    Pool* ents = &(gs->entities);
    Index i = pool_append(ents,e);
    Entity* self = pool_refer(ents,i);
    self->self_index = i;

    //printf("appeneded entity with id: %lu\n", i);
}


GlobalState gs_init(Vector2 window_size) {
    GlobalState s = {
        .entities           = pool_new(Entity),
        .window_dimensions  = window_size,
        .background         = GetColor(0x142024FF),
        .camera = {
            .offset     = {window_size.x / 2, window_size.y / 2},
            .target     = {0},
            .rotation   = 0,
            .zoom = 1,
        },
    };

    pool_resize(&s.entities, MAX_ENTITY_COUNT);
    return s;
}

void gs_clear(GlobalState* s) {
    pool_free(&(s->entities));
}

void draw_hud_debug(GlobalState* s) {
    hud_statuspanel(s);
}

void update_camera_input(GlobalState* s) {
    float mwf = GetMouseWheelMove();
    s->camera.zoom += mwf * ZOOM_SPEED;

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {

    }
}

bool IsShiftPlusKey(int key) {
    return (IsKeyPressed(KEY_LEFT_SHIFT) && IsKeyPressed(key));
}

Vector2 update_path_as_line(Path* p, Vector2* entity_position, float linear_velocity, bool debug) {
    Vector2 b,e;
    b = p->as_line.start;
    e = p->as_line.end;
 
    const float period = 300;
    const float amplitude = 25;

    float next_position = p->position + linear_velocity;

    Vector2 trajectory = Vector2Subtract(e,b);
    Vector2 trajectory_normalized = Vector2Normalize(trajectory);
    Vector2 trajectory_normal = Vector2Normal(trajectory_normalized);
    Vector2 point = Vector2Add(Vector2Scale(trajectory_normalized, p->position),b);
    Vector2 point_next = Vector2Add(Vector2Scale(trajectory_normalized, next_position),b);
    Vector2 cycle = Vector2Add(Vector2Scale(trajectory_normalized, period),b);

    float linear_length = Vector2Length(trajectory);


    if (p->position > linear_length)
        p->position = 0;

    float period_adjusted = ( period / (PI*2) ); // adjust period to match the float value
    float wave_current = sinf(p->position   /period_adjusted) * amplitude;
    float wave_next    = sinf(next_position /period_adjusted) * amplitude;

    Vector2 normal_point = Vector2Add(
            Vector2Scale(trajectory_normal, wave_current),
            point
    );
    Vector2 next_normal_point = Vector2Add(
            Vector2Scale(trajectory_normal, wave_next),
            point_next
    );

    Vector2 sine_vector = Vector2Subtract(next_normal_point, normal_point);
    Vector2 sine_vector_scaled = Vector2Scale(sine_vector,10);

    if (debug) {
        DrawLineEx(b,e,10,GREEN);
        DrawLineEx(b,cycle,10,ORANGE);
        DrawLineEx(normal_point,Vector2Add(sine_vector_scaled,normal_point),10,YELLOW);

        DrawCircleV(point,10,RED);
        DrawCircleV(next_normal_point,10,GREEN);
        DrawCircleV(normal_point,10,ORANGE);
    }

    p->position += linear_velocity;

    *entity_position = normal_point;
    return sine_vector;
}


Vector2 update_path(Entity* e, bool debug) {

    Path* p = &(e->travel_path);

    switch (p->mode) {
        case PATH_LINE: 
            {
                Vector2* position = &(e->pos);
                return update_path_as_line(p, position, e->linear_velocity, debug); 
            }
        default: ;
    }
}


Entity get_default_entity_fish(void) {
    Color fish_col = GetColor(0x108090FF);
    return ENTITY {
        .alive = true,
        .pos = VECTOR2 {0, 0}, // doesnt matter, since path handles the critter position (for now)
            .body = {
                { .size = 20, .bonelen = 25, .col = fish_col, .pos = {100,0} },
                { .size = 25, .bonelen = 25, .col = fish_col, .pos = {100,0} },
                { .size = 28, .bonelen = 25, .col = fish_col, .pos = {100,30}},
                { .size = 25, .bonelen = 25, .col = fish_col, .pos = {0,90}},
                { .size = 25, .bonelen = 30, .col = fish_col, .pos = {0,90}},
                { .size = 20, .bonelen = 30, .col = fish_col, .pos = {0,120}},
                { .size = 10, .bonelen = 30, .col = fish_col, .pos = {0,120}},
                { .size = 5, .bonelen = 30, .col = fish_col, .pos = {0,120}},
                //{ .size = 30, .col = RAYWHITE, .pos = {0,150}},
            },
            .joint_count = 8,


            .fins = {
                {0, {1,50,70}},
                {0, {5,40,50}},
                {0, {6,30,70}},

                // this was test case
                //{1, .as_backfin = {.begin = 1,.end = 4}    },

                {1, .as_backfin = {.begin = 1,.end = 3}    },
                {1, .as_backfin = {.begin = 4,.end = 5}    },
            }, 
            .fin_count = 5,

            .eye_size = 4,
            .eye_center_offset = 0.2,

            .travel_path = {
                .waving_amplitude = 25,
                .waving_period = 250,
                .as_line = { {-400,-450}, {450,450} }
            },
            .linear_velocity = 2,
    };
} 


void draw_hud_ui(GlobalState* gs) {
    const float bar_sizes[4] = {24,24,36,48};

    // render aquarium frame
    {

        Rectangle outer_outline = {0,0,UNFOLD_V2(gs->window_dimensions)}; 
        Rectangle inner_outline = {
            .x = outer_outline.x + bar_sizes[0],
            .y = outer_outline.y + bar_sizes[2],
            .width  = outer_outline.width   - bar_sizes[1] - bar_sizes[0],
            .height = outer_outline.height  - bar_sizes[3] - bar_sizes[2],
        };

        Color bg = ColorBrightness(gs->background,-0.1);

        // sectors
        DrawRectangleRec(RECTANGLE {
                0,0,
                bar_sizes[0],
                outer_outline.height
                }, bg);

        DrawRectangleRec(RECTANGLE {
                bar_sizes[0],0,
                outer_outline.width,
                bar_sizes[2]
                }, bg);

        DrawRectangleRec(RECTANGLE {
                outer_outline.x + outer_outline.width - bar_sizes[0],
                0,
                bar_sizes[1],
                outer_outline.height
                }, bg);

        DrawRectangleRec(RECTANGLE {
                .x = bar_sizes[0],
                .y = outer_outline.height  - bar_sizes[3],
                outer_outline.width - bar_sizes[0] - bar_sizes[1],
                bar_sizes[3]
                }, bg);

        DrawRectangleLinesEx(outer_outline,2,RAYWHITE);
        DrawRectangleLinesEx(inner_outline,1,RAYWHITE);
    }

    // render fish counter
    {
        const float font_size = 24;
        const char* text = TextFormat("fish counter: %i",(int) gs->entities.count);
        const int   text_len = MeasureText(text,font_size);
        const Vector2 counter_size = { text_len * 1.25, 30 };
        
        const Color bg      = ColorBrightness(gs->background,0.2);
        const Color outline = ColorBrightness(gs->background,1.0);
        
        Rectangle counter_body = {
            .x = bar_sizes[0] + counter_size.x/3,
            .y = gs->window_dimensions.y - bar_sizes[3] - (counter_size.y/2),
            .width = counter_size.x,
            .height = counter_size.y
        };
        
        

        // text
        DrawRectangleRounded(counter_body, 30, 8, 
                gs->rainbow_spawned ? ColorFromHSV(gs->tick,0.5,0.2) : bg
        );
        DrawRectangleRoundedLinesEx(counter_body, 30, 8,2, 
                gs->rainbow_spawned ? ColorFromHSV(gs->tick,0.1,1) : outline
        );
        DrawText( text,
                counter_body.x + (counter_body.width - text_len)/2,
                counter_body.y + (counter_body.height - font_size)/2,
                font_size,
                 outline
        );
    }
}

// all constants and macros are defined in "./src/defs.h"
int main(void) {
    Vector2 window_size = {800,600};
    GlobalState state = gs_init(window_size);
    Pool* ents = &(state.entities);

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    SetTargetFPS(FPS);
    InitWindow(UNFOLD_V2(window_size),"Window");
    


    //append_entity(&state, get_default_entity_fish());
    //Entity* e = pool_refer(ents,0);

    bool pause = true;

    // tick once to start
    update(&state);

    while(!WindowShouldClose()) {

        // force window ration for window managers
        SetWindowSize(UNFOLD_V2(window_size));

        //e->vel = DEFAULT_VELOCITY;


        if (IsKeyPressed(KEY_SPACE)) 
            state.pause = !state.pause;
        

        if (IsKeyPressed(KEY_SLASH)) 
            state.debug = !state.debug;

        if (IsKeyPressed(KEY_ENTER)) {
            Entity manual_spawn = get_default_entity_fish();
            manual_spawn.rainbow = true;
            append_entity(&state, manual_spawn);
        }

        if (state.debug)
            update_camera_input(&state);
        else state.camera.zoom = 1;

        BeginDrawing();
        
        BeginMode2D(state.camera);
      
        update(&state);
        draw(&state);
        EndMode2D();
        
        if (state.debug) draw_hud_debug(&state);
        else draw_hud_ui(&state);
        EndDrawing();
    }
    CloseWindow();

    gs_clear(&state);
}
