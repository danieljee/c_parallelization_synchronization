#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <time.h>
#include "shared.h"

Message *recv_message(int sock_fd)
{
    unsigned char recv_buf[1500];
    int received = recv(sock_fd, recv_buf, sizeof(recv_buf), 0);
    if (received == -1)
    {
        printf("Failed to recv game_state\n");
        exit(EXIT_FAILURE);
    }
    if (received == 0)
    {
        printf("Invalid data received from the server. Terminating\n");
        exit(EXIT_FAILURE);
    }

    return deserialize_msg(recv_buf); //need to free
}

const int create_connected_sock(char* hostname, const u_int16_t port)
{
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1)
    {
        printf("Failed to create socket\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr_in;
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(port);
    if (inet_pton(AF_INET, hostname, &addr_in.sin_addr.s_addr) != 1)
    {
        printf("Failed to resolve address\n");
        exit(EXIT_FAILURE);
    }

    if (connect(sock_fd, (struct sockaddr*) &addr_in, sizeof(addr_in)) == -1)
    {
        printf("Failed to connect: ");
        switch(errno)
        {
            case EACCES:
                printf("permission denied\n");
                break;
            case EADDRINUSE:
                printf("address in use\n");
                break;
            case EINPROGRESS:   
            case EALREADY:
                printf("previous connection still alive\n");
                break;
            case ENETUNREACH:
                printf("network unreachable\n");
                break;
            case ECONNREFUSED:
            default:
                printf("invalid address\n");
        }
        
        exit(EXIT_FAILURE);
    }
    return sock_fd;
}

void get_x_y_coords(Game_state *game_state, int *x, int *y)
{
    char coordinates[255];
    char *inputs[255];
    while(true)
    {
        
        printf("\nPlease provide x and y coordinates separated by a space. E.g (3 A):");
        if (scanf("%[^\n]s", coordinates) != 1)
        {
            printf("Invalid input. Try again.\n\n");
            continue;
        }
        getchar();
        const char *delim = " ";
        int i=0;
        inputs[i++] = strtok(coordinates, delim);
        while( (inputs[i] = strtok(NULL, delim)) != NULL) ++i;
        if (i != 2)
        {
            printf("Invalid input. Please provide exactly two values each for x and y coordinates.\n");
            continue;
        }    
        if (strlen(inputs[0]) > 1 || strlen(inputs[1]) > 1 || 
            (inputs[0][0] < 49 || inputs[0][0] > 57) || 
            (inputs[1][0] < 65 || inputs[1][0] > 73)
        )
        {
            printf("Invalid input. X value must be between 1 and 9, and Y value must be between A and I.\n");
            continue;
        }

        *x = inputs[0][0] - 48 - 1;
        *y = inputs[1][0] - 64 - 1;
        
        if (game_state->tiles[*x][*y].revealed)
        {
            printf("Already revealed. Choose a different coordinate\n");
            continue;
        }

        break;
    }
}

void reveal_tile(const int sock_fd, Game_state* game_state)
{
    int x, y;
    get_x_y_coords(game_state, &x, &y);
    char coord[255];
    sprintf(coord, "Reveal:%d %d", x, y);
    send(sock_fd, coord, strlen(coord), 0);
}

void handle_reveal(const int sock_fd, Game_state **game_state)
{
    Message *message = recv_message(sock_fd);
    switch(message->status)
    {
        case LOST:
            free(*game_state);
            (*game_state) = deserialize_game_state(message->data); // TODO Should ALWAYS call free on heap memory.
            display_game(*game_state, true);
            printf("\n@@@@@@@ Game Over! You hit a mine\n");
            free(*game_state);
            free(message);
            exit(EXIT_FAILURE);
        case ERROR:
            printf("\n%s\n", message->data);
            break;
        case TERMINATE:
            printf("Error occurred. Terminating\n");
            free(*game_state);
            free(message);
            exit(EXIT_FAILURE);
        case OK:
            free(*game_state);
            (*game_state) = deserialize_game_state(message->data);
    }
    free(message);
}

void place_flag(const int sock_fd, Game_state* game_state)
{
    int x, y;
    get_x_y_coords(game_state, &x, &y);
    char coord[255];
    sprintf(coord, "Flag:%d %d", x, y);
    send(sock_fd, coord, strlen(coord), 0);
}

void handle_place_flag(const int sock_fd, Game_state **game_state)
{
    Message *message = recv_message(sock_fd);
    switch(message->status)
    {
        case ERROR:
            printf("\n%s\n", message->data);
            break;
        case TERMINATE:
            printf("Error occurred. Terminating\n");
            free(message);
            free(*game_state);
            exit(EXIT_FAILURE);
        case OK:
            free(*game_state);
            (*game_state) = deserialize_game_state(message->data);
            break;
        case WIN:
            free(*game_state);
            (*game_state) = deserialize_game_state(message->data); 
            display_game(*game_state, false);
            time_t t = (*game_state)->end - (*game_state)->start;
            printf("\n@@@@@@@ Congrats! You won in %ld seconds!\n\n", t); //display time.

            free(message);
            free(*game_state);

            exit(EXIT_FAILURE);
    }
    free(message);
}

void quit(const int sock_fd)
{
    printf("@@@@@@@ Quit!\n");

    // TODO cleanup.
    close(sock_fd);

    exit(EXIT_FAILURE);
}

void start_game(const int sock_fd)
{
    printf("GAME STARTED!\n");
    
    Message *msg = recv_message(sock_fd);
    Game_state *game_state = deserialize_game_state(msg->data);
    free(msg);

    while(true)
    { 
        display_game(game_state, false);
        printf("Choose an option:\n<R> Reveal a tile\n<P> Place a flag\n<Q> Quit\nOption<R, P, Q>: ");
        char selected[255]; // does scanf exceed memory bound?
        if (scanf("%s", selected) != 1 || strlen(selected) != 1)
        {
            printf("Invalid input\n");
            continue;
        }
        getchar();
        switch(selected[0])
        {
            case 'R':
                reveal_tile(sock_fd, game_state);
                handle_reveal(sock_fd, &game_state);
                break;
            case 'P':
                place_flag(sock_fd, game_state);
                handle_place_flag(sock_fd, &game_state);
                break;
            case 'Q':
                free(game_state);
                quit(sock_fd);
                break;
            default:
                printf("Invalid option. Choose R, P, or Q\n");
        }
    }
}

void show_leaderboard(const int sock_fd)
{
    printf("=============================================================\n\n");
    Message* msg = recv_message(sock_fd);
    if (msg->status == ERROR)
    {
        printf("%s\n\n", msg->data);
    } else {
        struct Score *scores;
        int count = deserialize_scores(msg->data, &scores);
        free(msg);

        for (int i=0; i != count; ++i)
            printf("%s\t%ld seconds \t\t %d games won, %d games played\n", scores[i].name, scores[i].time, scores[i].won, scores[i].played);
    } 
    printf("=============================================================\n\n");
}

void start(const int sock_fd)
{
    printf("Please select:\n<1>Play game\n<2>Show leaderboard\n<3>Quit\n\nSelection Option (1-3):");
    char selected[255];
    if (scanf("%s", selected) != 1 || strlen(selected) != 1 || selected[0] < 49 || selected[0] > 51)
    {
        printf("Invalid input\n");
        return start(sock_fd);
    }

    char option_in_string[10];
    sprintf(option_in_string, "%d", selected[0] - 48);

    send(sock_fd, option_in_string, strlen(option_in_string), 0);

    switch(selected[0] - 48)
    {
        case 1:
            start_game(sock_fd);
            break;
        case 2:
            show_leaderboard(sock_fd);
            break;
        case 3:
            quit(sock_fd);
            break;
        default:
            printf("Invalid option. Try again.\n"); //TODO @ this leads to infinite loop
            return start(sock_fd);
    }
}

void login(char* hostname, const u_int16_t port)
{
    char username[30], password[30], username_pw[258];

    printf("@@@@@@@ This is a minesweeper game.\n@@@@@@@\n@@@@@@@ Please provide credentials to connect to server\n\n");
    printf("Username: ");
    scanf("%s", username);
    printf("Password: ");
    scanf("%s", password);

    const int sock_fd = create_connected_sock(hostname, port);

    sprintf(username_pw, "%s %s", username, password);
    
    send(sock_fd, username_pw, strlen(username_pw), 0);

    printf("Sent username and password: %s. Waiting...\n", username_pw);

    Message* msg = recv_message(sock_fd);
    if (msg->status >= SUCCESS)
    {
        printf("@@@@@@@ Successfully logged in\n\n");
        free(msg);
        start(sock_fd);
    } else {
        printf("Failed to login. Reason: %s\n", msg->data);
        free(msg);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char** argv)
{
    if (argc < 3) 
    {
        printf("Usage: ./client <server ip> <port>\n");
        exit(EXIT_FAILURE);
    }

    char* hostname = argv[1];

    long port = strtol(argv[2], NULL, 10);
    if (errno == ERANGE || port > 65535 || port < 1024)
    {
        printf("Invalid port number. Should be (1024 ~ 65535)\n");
        exit(EXIT_FAILURE);
    }

    login(hostname, port);

    return 0;
}