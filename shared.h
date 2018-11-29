#ifndef SHARED_HEADER_FILE
#define SHARED_HEADER_FILE
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define MAX_THREADS 10
#define NUM_TILES_X 9
#define NUM_TILES_Y 9

#define htonll(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))

typedef enum {
    ERROR,
    INVALID,
    LOST,
    TERMINATE,
    WIN,
    SUCCESS = 200,
    OK
} game_status;

typedef struct {
    game_status status;
    size_t data_len;
    unsigned char* data;
} Message;

typedef struct {
    int adjacent_mines;
    bool revealed;
    bool is_mine;
    int x;
    int y;
} Tile;

typedef struct {
    int mine_num;
    time_t start;
    time_t end;
    Tile tiles[NUM_TILES_X][NUM_TILES_Y];
} Game_state;

struct Score{
    time_t time;
    int played;
    int won;
    char name[255];
    struct Score *next;
};

void initialize_tiles(Game_state*);

unsigned char* serialize_game_state(Game_state*, bool);

Game_state* deserialize_game_state(unsigned char*);

unsigned char* serialize_scores(struct Score*, int);

int deserialize_scores(unsigned char*, struct Score**);

void display_game(Game_state*, bool);

unsigned char* serialize_msg(Message*);

Message* deserialize_msg(unsigned char*);

size_t get_message_size(Message*);

void send_data_str(int, game_status, unsigned char*);

void send_data(int, game_status, unsigned char*, size_t);

#endif