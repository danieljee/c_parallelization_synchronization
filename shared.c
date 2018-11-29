#include "shared.h"

void initialize_tiles(Game_state* game_state)
{
    for (int x=0; x != NUM_TILES_X; ++x)
    {
        for (int y=0; y != NUM_TILES_Y; ++y)
        {
            game_state->tiles[x][y].adjacent_mines = 0;
            game_state->tiles[x][y].is_mine = false;
            game_state->tiles[x][y].revealed = false;
            game_state->tiles[x][y].x = x;
            game_state->tiles[x][y].y = y;
        }
    }
} 

unsigned char* serialize_game_state(Game_state* game_state, bool finished)
{
    size_t len = sizeof(int) + sizeof(time_t) + sizeof(game_state->tiles);  
    unsigned char* serialized = (unsigned char*) malloc(sizeof(char) * len);
    unsigned char* mines = serialized;
    unsigned char* start = serialized + sizeof(int);
    unsigned char* end = start + sizeof(time_t);
    unsigned char* tiles = end + sizeof(time_t);

    *((int*) mines) = htonl(game_state->mine_num);
    *((time_t*) start) = htonll(game_state->start);
    *((time_t*) end) = htonll(game_state->end);

    for (int y=0; y != NUM_TILES_Y; ++y)
    {
        for (int x=0; x != NUM_TILES_X; ++x)
        {
            bool is_mine = (finished || game_state->tiles[x][y].revealed)? game_state->tiles[x][y].is_mine: false;
            bool reveal = finished ? true: game_state->tiles[x][y].revealed;

            *((int*)tiles) = htonl(game_state->tiles[x][y].adjacent_mines);
            tiles += sizeof(int);
            *((bool*)tiles) = htons(reveal);
            tiles += sizeof(bool);
            *((bool*)tiles) = htons(is_mine);
            tiles += sizeof(bool);
            *((int*)tiles) = htonl(game_state->tiles[x][y].x);
            tiles += sizeof(int);
            *((int*)tiles) = htonl(game_state->tiles[x][y].y);
            tiles += sizeof(int);
        }
    }
    return serialized;
}

Game_state* deserialize_game_state(unsigned char* data)
{
    Game_state* gs = malloc(sizeof(Game_state));
    
    unsigned char* mine_num = data;
    unsigned char* start = data + sizeof(int);
    unsigned char* end = start + sizeof(time_t);
    unsigned char* tiles = end + sizeof(time_t);

    gs->mine_num = ntohl(*((int*)mine_num));
    gs->start = ntohll(*((time_t*)start));
    gs->end = ntohll(*((time_t*)end));
    for (int y=0; y != NUM_TILES_Y; ++y)
    {
        for (int x=0; x != NUM_TILES_X; ++x)
        {
            gs->tiles[x][y].adjacent_mines = ntohl(*((int*)tiles));
            tiles += sizeof(int);
            gs->tiles[x][y].revealed = ntohs(*((bool*)tiles));
            tiles += sizeof(bool);
            gs->tiles[x][y].is_mine = ntohs(*((bool*)tiles));
            tiles += sizeof(bool);
            gs->tiles[x][y].x = ntohl(*((int*)tiles));
            tiles += sizeof(int);
            gs->tiles[x][y].y = ntohl(*((int*)tiles));
            tiles += sizeof(int);
        }
    }

    return gs;
}

unsigned char* serialize_scores(struct Score *scores, int count)
{  

    unsigned char* serialized = malloc(count * sizeof(struct Score) + sizeof(int));

    *((int*) serialized) = htonl(count);
    unsigned char* asdf = serialized + sizeof(int);
    for (struct Score* s = scores; s != NULL; s = s->next)
    {
        *((time_t*) asdf) = htonll(s->time);
        asdf += sizeof(time_t);
        *((int*) asdf) = htonl(s->played);
        asdf += sizeof(int);
        *((int*) asdf) = htonl(s->won);
        asdf += sizeof(int);
        memcpy(asdf, s->name, sizeof(s->name));
        asdf += sizeof(s->name);
        asdf += sizeof(s->next);
    }
    return serialized;
}

int deserialize_scores(unsigned char* data, struct Score** scores)
{
    int count = ntohl(*((int *) data));
    data += sizeof(int);
    (*scores) = malloc(count * sizeof(struct Score));

    for (int i=0; i != count; ++i)
    {
        (*scores)[i].time = ntohll(*((time_t*)data));
        data += sizeof(time_t);
        (*scores)[i].played = ntohl(*((int*)data));
        data += sizeof(int);
        (*scores)[i].won = ntohl(*((int*)data));
        data += sizeof(int);
        memcpy((*scores)[i].name, data, sizeof((*scores)[i].name));
        data += sizeof((*scores)[i].name);
        (*scores)[i].next = NULL;
        data += sizeof((*scores)[i].next);
    }
    return count;
}

void display_game(Game_state* game_state, bool lost)
{
    printf("Remaining Mines: %d\n", game_state->mine_num);
    printf("    1 2 3 4 5 6 7 8 9\n");
    printf("---------------------\n");
    for (int y=0; y != NUM_TILES_Y; ++y)
    {    
        printf("%c | ", 65 + y);
        for (int x=0; x != NUM_TILES_X; ++x)
        {
            if (game_state->tiles[x][y].is_mine){
                if (game_state->tiles[x][y].revealed && !lost)
                    printf("+ "); //if revealed && is_mine //is flagged.
                else 
                    printf("* ");
            } else if (game_state->tiles[x][y].revealed && !lost){
                printf("%d ", game_state->tiles[x][y].adjacent_mines);  
            } else {
                printf("  ");
            }
        }
        printf("\n");
    }
}

unsigned char* serialize_msg(Message* msg)
{
    size_t len = get_message_size(msg); 
    unsigned char* serialized = malloc(len);
    
    unsigned char* _status = serialized;
    unsigned char* _len = _status + sizeof(game_status);
    unsigned char* _data = _len + sizeof(size_t);

    *((game_status*)_status) = htonl(msg->status);
    *((size_t*)_len) = htonll(msg->data_len);
    memcpy(_data, msg->data, msg->data_len);
    return serialized;
}

Message* deserialize_msg(unsigned char* data)
{
    Message *msg = (Message*) malloc(sizeof(Message));

    unsigned char* _status = data;
    unsigned char* _len = _status + sizeof(game_status);
    unsigned char* _data = _len + sizeof(size_t);

    msg->status = ntohl( *((game_status*)_status) );
    msg->data_len = ntohll( *((size_t*) _len) );
    msg->data = malloc(msg->data_len);
    memcpy(msg->data, _data, msg->data_len);
    
    return msg;
}

size_t get_message_size(Message* msg)
{
    return sizeof(msg->status) + sizeof(msg->data_len) + sizeof(msg->data) + msg->data_len;
}

void send_data_str(int conn_fd, game_status status, unsigned char* data)
{
    Message msg;
    msg.status = status;
    msg.data = data;
    msg.data_len = strlen(msg.data) + 1; //add 1 since strlen omits null terminator.
    unsigned char *to_send = serialize_msg(&msg);

    send(conn_fd, to_send, get_message_size(&msg), 0);
    free(to_send);
}

void send_data(int conn_fd, game_status status, unsigned char* data, size_t len)
{
    Message msg;
    msg.status = status;
    msg.data_len = len;
    msg.data = data;
    unsigned char *to_send = serialize_msg(&msg);
    send(conn_fd, to_send, get_message_size(&msg), 0);
    free(to_send);
}