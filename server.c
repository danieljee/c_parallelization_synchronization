#define _XOPEN_SOURCE
#define _GNU_SOURCE 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>
#include "thread_pool.h"
#include "shared.h"
#include <time.h>
#include <signal.h>

#define NUM_MINES 10
#define RAMDOM_NUMBER_SEED 42

typedef struct {
    int conn_fd;
    char *name;
    Game_state *game_state;
} conn_args;

int score_records = 0;
struct Score *scores = NULL; //TODO clean up. Nodes are allocated memory.
int reader_count = 0;
Thread_pool *thread_pool;

pthread_mutex_t rand_mutex, reader_mutex, rw_mutex; // TODO clean up

void send_leaderboard(const int conn_fd)
{
    
    printf("send leaderboard. score_records: %d \n", score_records);
    if (score_records == 0)
    {
        send_data_str(conn_fd, ERROR, "There is no information currently stored in the leaderboard.");
        return;
    }

    pthread_mutex_lock(&reader_mutex);
    ++reader_count;
    if (reader_count == 1)
        pthread_mutex_lock(&rw_mutex);

    pthread_mutex_unlock(&reader_mutex);
    unsigned char* serialized = serialize_scores(scores, score_records);
    pthread_mutex_lock(&reader_mutex);
    --reader_count;
    if (reader_count == 0)
        pthread_mutex_unlock(&rw_mutex);
    pthread_mutex_unlock(&reader_mutex);
    
    size_t size = score_records * sizeof(struct Score) + sizeof(int);
    send_data(conn_fd, OK, serialized, size);
    free(serialized);
}
/*
    -1: new score should be placed before existing
    0: same
    1: new score should be placed after existing
*/
int compare_score(struct Score *existing, struct Score *new_score)
{
    if (existing->time == new_score->time)
    {
        if (existing->won == new_score->won) 
        {
            if (strcmp(existing->name, new_score->name) == 0)
            {
                return 0;//Duplicate!
            }

            if (strcmp(existing->name, new_score->name) > 0) {
                return -1; // new_score should be placed before since its name comes before existing score's name
            } else {
                return 1;
            }
        }

        if (existing->won > new_score->won)
        {
            return -1; // new_score should be placed before since it has lower winning count
        } else {
            return 1; // new_score should be placed after since it has higher winning count
        }
    }

    if (existing->time < new_score->time)
        return -1;
    else {
        return 1;
    }
}

//returns NULL if there are no non_duplicates
struct Score * get_first_non_duplicate(struct Score * scores_head, struct Score *score)
{
    if (scores_head == NULL)
        return NULL;
    if (compare_score(scores_head, score) == 0)
        return get_first_non_duplicate(scores_head->next, score);
    else return scores_head;
}

struct Score * get_last_duplicate(struct Score * scores_head, struct Score *score)
{
    if (scores_head->next == NULL) return scores_head;
    if (compare_score(scores_head->next, score) == 0)
        return get_first_non_duplicate(scores_head->next, score);
    else return scores_head;
}

void place_score(struct Score ** scores_head, struct Score *new_score)
{
    struct Score *prev = NULL;
    printf("place_score head: %s  new: %s\n", (*scores_head)->name, new_score->name);
    for(struct Score *s = (*scores_head); s != NULL; s = s->next)
    {
        switch(compare_score(s, new_score))
        {
            case -1:
                printf("%s goes before %s\n", new_score->name, s->name);

                if (prev == NULL) {
                    scores = new_score;
                    new_score->next = s;
                } else {
                    prev->next = new_score;
                    new_score->next = s;
                }
                
                return;
            case 1:
                printf("%s goes after %s\n", new_score->name, s->name);
                break;
            case 0:
                printf("duplicate!\n");
                if (s->next == NULL) { 
                    printf("%s is the last node\n", s->name);
                    s->next = new_score;
                    new_score->next = NULL;
                    return;
                } else {
                    struct Score *first_non_dupe = get_first_non_duplicate(s, new_score);
                    if (prev == NULL) {
                        printf("prev is null\n");

                        if (first_non_dupe == NULL || compare_score(first_non_dupe, s) == -1) {
                            printf("subsequent scores are all duplicates or first none dupe goes after\n");
                        } else {
                            printf("Got first none duplicate: %s\n", first_non_dupe->name);
                            (*scores_head) = first_non_dupe;
                            while(s != first_non_dupe) {
                                struct Score *next = s->next;
                                place_score(&first_non_dupe, s);
                                s = next;
                            }
                            
                            place_score(&first_non_dupe, new_score);
                            return;
                        }
                        
                    } else {
                        printf("prev is not null\n");
                       if (first_non_dupe == NULL || compare_score(first_non_dupe, s) == -1) {
                            printf("subsequent scores are all duplicates or first none dupe goes after\n");
                       } else {
                            printf("Got first none duplicate: %s\n", first_non_dupe->name);
                            /*
                                If the head as duplicates, last duplicate of the head must link with first non_duplicate.
                            */
                            if (compare_score((*scores_head), (*scores_head)->next) == 0) {
                                get_last_duplicate((*scores_head)->next, (*scores_head))->next = first_non_dupe;
                            } else {
                                (*scores_head)->next = first_non_dupe; 
                            }
                
                            while(s != first_non_dupe) {
                               struct Score *next = s->next;
                               printf("beep\n");
                                place_score(&first_non_dupe, s);
                                s = next;
                                printf("boop\n");
                            }
                            place_score(&first_non_dupe, new_score);
                            return;
                       }
                       
                    }
                }  
        }

        prev = s;
    }

    new_score->next = NULL;
    prev->next = new_score;
    printf("%s is place at the end after %s\n", new_score->name, prev->name);
}

void record_score(char *name, time_t t, bool lost)
{
    pthread_mutex_lock(&rw_mutex);
    if (lost)
    {
        for(struct Score *s = scores; s != NULL; s = s->next)
            if (strcmp(s->name, name) == 0) ++s->played;
    }
    else 
    {
        ++score_records;
        int played = 1, won = 1;
        for(struct Score *s = scores; s != NULL; s = s->next)
        {
            if (strcmp(s->name, name) == 0)
            {
                played = ++s->played;
                won = ++s->won;
            }
        }

        struct Score *new_score = malloc(sizeof(struct Score));
        strcpy(new_score->name, name);
        
        new_score->played = played;
        new_score->won = won;
        new_score->time = t;
        new_score->next = NULL;
        
        if (scores == NULL)
            scores = new_score;
        else
            place_score(&scores, new_score);
    }
    pthread_mutex_unlock(&rw_mutex);
}

bool tile_contains_mine(Game_state* game_state, int x, int y)
{
    return game_state->tiles[x][y].is_mine;
}

Tile** get_adjacent_tiles(Game_state* game_state, Tile** tiles, int x, int y)
{
    int i = 0;
    if ((x + 1) < NUM_TILES_X)
    {
        tiles[i++] = &game_state->tiles[x+1][y];
        if ((y + 1) < 9) tiles[i++] = &game_state->tiles[x+1][y+1];
        if ((y - 1) >= 0) tiles[i++] = &game_state->tiles[x+1][y-1];
    }
    if ((x - 1) >= 0)
    {
        tiles[i++] = &game_state->tiles[x-1][y];
        if ((y + 1) < NUM_TILES_Y) tiles[i++] = &game_state->tiles[x-1][y+1];
        if ((y - 1) >= 0) tiles[i++] = &game_state->tiles[x-1][y-1];
    }
    if ((y + 1) < NUM_TILES_Y) tiles[i++] = &game_state->tiles[x][y+1];
    if ((y - 1) >= 0) tiles[i++] = &game_state->tiles[x][y-1];
    tiles[i] = NULL;
    return tiles;
}

void place_mines(Game_state* game_state)
{
    for (int i=0; i != NUM_MINES; ++i)
    {
        int x, y;
        do {
            pthread_mutex_lock(&rand_mutex);
            x = rand() % NUM_TILES_X;
            y = rand() % NUM_TILES_Y;
            pthread_mutex_unlock(&rand_mutex);
        } while (tile_contains_mine(game_state, x, y));
        game_state->tiles[x][y].is_mine = true;

        Tile* tiles[9];
        get_adjacent_tiles(game_state, tiles, x, y);
        for (int i=0; tiles[i] != NULL; ++i) {
            if (tiles[i]->is_mine)
            {
                tiles[i]->adjacent_mines = 0;
            } else {
                ++tiles[i]->adjacent_mines;
            }
        }
    }
}

void reveal_tile(conn_args *args, Tile *tile)
{
    if (tile->revealed || tile->is_mine) return;     
    else {
        tile->revealed = true;
        if (tile->adjacent_mines == 0)
        {
            Tile* tiles[9];
            get_adjacent_tiles(args->game_state, tiles, tile->x, tile->y);
            for (int i=0; tiles[i] != NULL; ++i) reveal_tile(args, tiles[i]);   
        }
    }
}

void start_game(conn_args *args)
{
    printf("Started game\n");
    Game_state game_state;
    game_state.start = time(NULL);
    game_state.mine_num = NUM_MINES;
    initialize_tiles(&game_state);
    place_mines(&game_state);
    display_game(&game_state, true); 
    args->game_state = &game_state;

    unsigned char* serialized_game_state = serialize_game_state(&game_state, false);
    send_data(args->conn_fd, OK, serialized_game_state, sizeof(game_state.mine_num)+sizeof(game_state.tiles));
    free(serialized_game_state);
    
    while(true)
    {
        char recv_buf[255];
        memset(recv_buf, 0, sizeof recv_buf);
        int received = recv(args->conn_fd, recv_buf, sizeof(recv_buf), 0);
        if (received == -1) 
        {
            printf("Failed to recv errno: %d. Terminating connection.\n", errno);
            return;
        }
        if (received == 0)
        {
            printf("Client terminated\n");
            return;
        }
        printf("Received: %s\n", recv_buf);
        
        const char *delim = ": ";
        char *action = strtok(recv_buf, delim);

        if (action == NULL) {
            printf("Invalid message from client. Terminating connection.\n");
            return;
        }

        if (strcmp(action, "Reveal") == 0)
        {
            long x = strtol(strtok(NULL, delim), NULL, 10); //Find other routines that return 4bytes.
            long y = strtol(strtok(NULL, delim), NULL, 10);
            Tile* tile = &game_state.tiles[x][y];
            if (tile->revealed)
                {
                    send_data_str(args->conn_fd, ERROR, "The tiles is already revealed");
                    continue;
                }
            if (tile->is_mine) 
                {
                    unsigned char *serialized_game_state = serialize_game_state(&game_state, true);
                    send_data(args->conn_fd, LOST, serialized_game_state, sizeof(game_state.mine_num)+sizeof(game_state.tiles));
                    free(serialized_game_state); 
                    record_score(args->name, 0, true);
                    return;   
                }

            reveal_tile(args, tile);
            unsigned char *serialized_game_state = serialize_game_state(&game_state, false);
            send_data(args->conn_fd, OK, serialized_game_state, sizeof(game_state.mine_num)+sizeof(game_state.tiles));
            free(serialized_game_state);
        } 
        else if (strcmp(action, "Flag") == 0)
        {
            long x = strtol(strtok(NULL, delim), NULL, 10); //Find other routines that return 4bytes.
            long y = strtol(strtok(NULL, delim), NULL, 10);
            Tile* tile = &game_state.tiles[x][y];
            if (tile->revealed)
            {
                send_data_str(args->conn_fd, ERROR, "The tiles is already revealed");
                continue;
            }
            if (!tile->is_mine) 
            {
                send_data_str(args->conn_fd, ERROR, "The tiles does not contain mine");
                continue; 
            }   

            tile->revealed = true;
            if (--game_state.mine_num == 0)
            {
                game_state.end = time(NULL);
                unsigned char *serialized_game_state = serialize_game_state(&game_state, true);
                send_data(args->conn_fd, WIN, serialized_game_state, sizeof(game_state.mine_num)+sizeof(game_state.tiles));
                free(serialized_game_state);
                record_score(args->name, game_state.end - game_state.start, false);
                return;
            }

            unsigned char *serialized_game_state = serialize_game_state(&game_state, false);
            send_data(args->conn_fd, OK, serialized_game_state, sizeof(game_state.mine_num)+sizeof(game_state.tiles));
            free(serialized_game_state);
        } 
        else return;
    }
}

bool auth(conn_args* args)
{
    int conn_fd = args->conn_fd;
    char recv_buf[255];
    char *username, *password;
    memset(recv_buf, 0, sizeof recv_buf);
    if (recv(conn_fd, recv_buf, sizeof(recv_buf), 0) == -1)
    {
        printf("Failed to recv\n");
        return false;
    }

    const char* delim = " \t\n";
    username = strtok(recv_buf, delim);
    password = strtok(NULL, delim);

    if (username == NULL || password == NULL)
    {
        send_data_str(conn_fd, ERROR, "Invalid username or password");

        return false; 
    }

    FILE *fd = fopen("./Authentication.txt", "r"); //@file descriptor

    char line[255];
    //skip the first line.
    fgets(line, sizeof line / sizeof(char), fd);
    while (fgets(line, sizeof line / sizeof(char), fd) != NULL)
    {
        const char *db_username = strtok(line, delim);
        const char *db_password = strtok(NULL, delim);
        if (strcmp(username, db_username) == 0 && strcmp(password, db_password) == 0)
        {
            long len = strlen(username) + 1;
            args->name = malloc(len);
            strcpy(args->name, username);
            send_data_str(conn_fd, SUCCESS, "Auth success");

            return true;
        }

    }
    fclose(fd);

    send_data_str(conn_fd, ERROR, "User is not registered");

    return false;
}

void cleanup(void *_args)
{
    printf("-- cleaning up thread\n");
    fcloseall();
    conn_args* args = (conn_args*) _args;
    if (args != NULL) {
        if (args->name != NULL) free(args->name);
        free(args);;
    }   
    printf("-- finished cleaning up thread\n");
}

void handle_connection(void* _args)
{
    conn_args* args = (conn_args*) _args;
    pthread_cleanup_push(cleanup, _args);
    printf("Started handling client %d\n", args->conn_fd);

    if (auth(args)) 
    {
        char* user_selection[255];
        if (recv(args->conn_fd, user_selection, sizeof(user_selection), 0) == -1)
        {
            printf("Failed to recv user selection\n");
            return;
        }

        long option = strtol((const char*)user_selection, NULL, 10);
        printf("Option: %ld\n", option);
        switch(option)
        {
            case 1:
                start_game(args);
                break;
            case 2:
                send_leaderboard(args->conn_fd);
                break;
            case 3:
                break;
                //do nothing
                
        }
    }
    printf("conn_fd %d Done!\n", args->conn_fd);
    pthread_cleanup_pop(1);
}

void accept_connection(const int sock_fd)
{
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);

    while(true)
    {
        conn_args *cn_args = malloc(sizeof(conn_args));
        printf("waiting for connection\n");

        if ((cn_args->conn_fd = accept(sock_fd, (struct sockaddr*) &addr, &addr_size)) == -1)
        {
            printf("Failed to accept");
            exit(EXIT_FAILURE);
        }

        printf("Connection fd: %d\n", cn_args->conn_fd);

        submit_task(thread_pool, handle_connection, cn_args);
    }
}

void start_listen(const int sock_fd)
{
    printf("Listening...\n");
    
    if (listen(sock_fd, SOMAXCONN) == -1)
    {
        printf("Failed to listen");
        exit(EXIT_FAILURE);
    }
    accept_connection(sock_fd);
}

void start(const uint16_t port)
{   
    srand(RAMDOM_NUMBER_SEED);

    thread_pool = create_thread_pool(MAX_THREADS);
    
    int sock_fd;
    int reuse = 1;
    struct sockaddr_in addr_in;
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(port);
    addr_in.sin_addr.s_addr = INADDR_ANY;

    pthread_mutex_init(&rand_mutex, NULL);
    pthread_mutex_init(&reader_mutex, NULL);
    pthread_mutex_init(&rw_mutex, NULL);

    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf("Failed to create socket\n");
        exit(EXIT_FAILURE);
    }

    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));

    if (bind(sock_fd, (struct sockaddr*)&addr_in, sizeof(addr_in))  != 0)
    {

        switch (errno)
        {
            case EADDRINUSE:
                printf("Failed to bind socket: Address in use\n");
                break;
            case EINVAL:
                printf("Failed to bind socket: Socket is already bound\n");
                break;
            default:
                printf("Failed to bind socket errno %d\n", errno);
        }

        close(sock_fd);
        
        exit(EXIT_FAILURE);
    }
    start_listen(sock_fd);
}

void handle_sigint(int sig)
{
    printf("Sigint received. Cleaning up.\n");
    terminate(thread_pool);
    printf("Terminated thread pool\n");
    if (scores != NULL) {
        struct Score * prev = scores, *cur;
        scores = scores->next;
        for(cur = scores; cur != NULL; cur = cur->next)
        {
            free(prev);
            prev = cur;
        }
        free(prev);
    }
    
    printf("Clean up finished. Exiting...\n");
    signal(sig, SIG_DFL);
    raise(sig);
}

int main(int argc, char** argv)
{   
    long port = 12345;
    if (argc > 1) port = strtol(argv[1], NULL, 10);
    
    if (errno == ERANGE || port > 65535 || port < 1024)
    {
        printf("Invalid port number. Should be (1024 ~ 65535)\n");
        exit(EXIT_FAILURE);
    }

    // struct sigaction new_action;
    // new_action.sa_handler = handle_sigint;
    // sigemptyset(&new_action.sa_mask);
    // sigaddset(&new_action.sa_mask, SIGTERM);
    // pthread_sigmask(SIG_BLOCK, &new_action.sa_mask, NULL);
    // new_action.sa_flags = 0;

    // if (sigaction(SIGINT, &new_action, NULL) == -1) {
    //     signal(SIGINT, handle_sigint);
    // }

    start(port);

    return 0;
}
