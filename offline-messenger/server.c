#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <stdint.h>
#include <sqlite3.h>

#define PORT 2908
#define NMAX 1024
#define db_accounts "database_accounts.db"
#define db_mess "messages.db"

extern int errno;

typedef struct thData {
    char username[64];
	int idThread;
	int cl;
}thData;

thData *clients[100];

char message_for_client[NMAX];
char message_from_client[NMAX];

static void *treat(void *);
void answer(void *);

struct UserInfo {
    char username[64];
    int found;
    int is_blacklisted;
};

int check_login_callback(void *data, int argc, char **argv, char **col_names) 
{
    struct UserInfo *user_info = (struct UserInfo *)data;
    user_info->found = 1;
    if (argc > 0) 
    {
        strncpy(user_info->username, argv[0], 63);
        user_info->username[63] = '\0';
    }
    if (argc > 1) {
        user_info->is_blacklisted = atoi(argv[1]);
    }
    return 0;
}

int check_login_credentials(const char * email, const char * password, char * username)
{
    sqlite3 *db;
    struct UserInfo user_info;
    user_info.found = 0;
    user_info.is_blacklisted = 0;
    int rc = sqlite3_open(db_accounts, &db);
    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return errno;
    }
    char select_query[NMAX];
    sprintf(select_query, "SELECT username, blacklist FROM users WHERE email='%s' AND password='%s';", email, password);

    sqlite3_exec(db, select_query, check_login_callback, &user_info, 0);
    sqlite3_close(db);
    strncpy(username, user_info.username, 64);
    if (!user_info.found)
        return 0;
    else if (user_info.found && !user_info.is_blacklisted)
        return 1;
    else if (user_info.found && user_info.is_blacklisted)
        return 2;
    return -1;
}

int check_existing_callback(void *exists, int argc, char **argv, char **col_names) 
{
    int *exists_flag = (int *)exists;
    
    if (argc > 0 && argv[0] != NULL) 
    {
        int count = atoi(argv[0]);
        *exists_flag = (count > 0) ? 1 : 0;
    } 
    else *exists_flag = 0;
    return 0;
}

int check_existing(sqlite3 *db, const char *field, const char *value) // check if a field and a value exist in users DB
{
    char check_query[NMAX];
    sprintf(check_query, "SELECT COUNT(*) FROM users WHERE %s='%s';", field, value);
    int exists_flag = 0;
    int rc = sqlite3_exec(db, check_query, check_existing_callback, &exists_flag, 0);

    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Failed to execute SELECT query: %s\n", sqlite3_errmsg(db));
        return errno;
    }
    return exists_flag;
}

void insert_admin()
{
    sqlite3 *db;
    int rc = sqlite3_open(db_accounts, &db);
    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        perror("[server] Error at sqlite3_open.\n");
    }
    char *create_table_query = "CREATE TABLE IF NOT EXISTS users ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                        "username TEXT NOT NULL,"
                        "email TEXT NOT NULL,"
                        "password TEXT NOT NULL,"
                        "blacklist INTEGER NOT NULL);";

    rc = sqlite3_exec(db, create_table_query, 0, 0, 0);

    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Failed to create table: %s\n", sqlite3_errmsg(db));
        perror("[server] Error at sqlite3_create.\n");
        sqlite3_close(db);
    }  
    if (!check_existing(db, "username", "admin") && !check_existing(db, "email", "admin@admin.com"))
    {
        char insert_query[NMAX];
        sprintf(insert_query, "INSERT INTO users (username, email, password, blacklist) VALUES ('%s', '%s', '%s', '%d');", "admin", "admin@admin.com", "123", 0);
        rc = sqlite3_exec(db, insert_query, 0, 0, 0);
        if (rc != SQLITE_OK) 
        {
            fprintf(stderr, "Failed to insert data: %s\n", sqlite3_errmsg(db));
            perror("[server] Error at sqlite3_exec.\n");
            sqlite3_close(db);
        }
        printf("[server] Admin inserted successfully!\n");
    }
    else printf("[server] Admin is already in database.\n");
    sqlite3_close(db);
}

int register_account(const char* username, const char * email, const char * password)
{
    if (!strlen(username) || strchr(username, ' '))
        return -3; // invalid username
    if (!strlen(email) || strchr(email, ' ') || strchr(email, '@') == NULL || strchr(email, '.') == NULL)    
        return -2; // email invalid
    if (!strlen(password) || strchr(password, ' '))
        return -1; // no password

    sqlite3 *db;
    int rc = sqlite3_open(db_accounts, &db);
    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        perror("[server] Error at sqlite3_open.\n");
        return errno;
    }
    char *create_table_query = "CREATE TABLE IF NOT EXISTS users ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "username TEXT NOT NULL,"
                    "email TEXT NOT NULL,"
                    "password TEXT NOT NULL,"
                    "blacklist INTEGER NOT NULL);";

    rc = sqlite3_exec(db, create_table_query, 0, 0, 0);

    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Failed to create table: %s\n", sqlite3_errmsg(db));
        perror("[server] Error at sqlite3_create.\n");
        sqlite3_close(db);
        return errno;
    } 

    if (check_existing(db, "username", username))
    {
        sqlite3_close(db);
        return 0;
    }

    if (check_existing(db, "email", email))
    {
        sqlite3_close(db);
        return 1;
    }
    
    char insert_query[NMAX];
    sprintf(insert_query, "INSERT INTO users (username, email, password, blacklist) VALUES ('%s', '%s', '%s', '%d');", username, email, password, 0);
    rc = sqlite3_exec(db, insert_query, 0, 0, 0);
    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Failed to insert data: %s\n", sqlite3_errmsg(db));
        perror("[server] Error at sqlite3_exec.\n");
        sqlite3_close(db);
        return errno;
    }
    printf("[server] Data inserted successfully!\n");
    sqlite3_close(db);
    return 2;
}

void add_client(thData *client) // add client if a user logged in
{
    for (int i = 0; i < 100; i++)
        if (clients[i] == NULL)
        {
            clients[i] = client;
            break;
        }
}

void remove_client(thData *client) // remove client if the user quit/logout
{
    for(int i = 0; i < 100; i++)
        if(clients[i] == client)
        {
            clients[i] = NULL;
            break;
        }
}

int get_users_callback(void *data, int argc, char **argv, char **col_names) 
{
    char *get_data = (char *)data;
    for (int i = 0; i < argc; i++) {
        strcat(get_data, argv[i]);
        strcat(get_data, "\n");
    }
    return 0;
}

void get_users() // get all the users that registered
{
    sqlite3 * db;
    int  rc = sqlite3_open(db_accounts, &db);
    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        perror("[server] Error at sqlite3_open.\n");
    }
    char *select_query = "SELECT username FROM users;";
    char get_data[NMAX] = {0};
    strcat(get_data, "The existing users are:\n");
    rc = sqlite3_exec(db, select_query, get_users_callback, get_data, 0);

    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Failed to execute SELECT query: %s\n", sqlite3_errmsg(db));
        perror("[server] Error at sqlite3_exec.\n");
    }
    strncpy(message_for_client, get_data, strlen(get_data));
    sqlite3_close(db);
}

void get_online_users() // get the online users -> clients[]
{
    char get_data[NMAX];
    bzero(get_data, NMAX);
    strcat(get_data, "The online users are:\n");
    for (int i = 0; i < 100; i++)
    {
        if (clients[i] != NULL)
        {
            strcat(get_data, clients[i]->username);
            strcat(get_data, "\n");
        }
    }
    strncpy(message_for_client, get_data, strlen(get_data));
}

int get_id_msg(const char* from_username, const char* to_username) // get the highest ID from the messages in DB
{
    sqlite3 * db;
    int  rc = sqlite3_open(db_mess, &db);
    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        perror("[server] Error at sqlite3_open.\n");
    }  

    char query[NMAX];
    bzero(query, NMAX);
    snprintf(query, sizeof(query), "SELECT COUNT(*) FROM messages WHERE from_user = ? AND to_user = ?");
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return rc;
    }

    sqlite3_bind_text(stmt, 1, from_username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, to_username, -1, SQLITE_STATIC);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) 
    {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return count;
}

int exist(const char * username) // check if an username exists in the users DB
{
    sqlite3 * db;
    int  rc = sqlite3_open(db_accounts, &db);
    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        perror("[server] Error at sqlite3_open.\n");
    }
    if (!check_existing(db, "username", username))
    {
        sqlite3_close(db);
        return 0;
    }
    sqlite3_close(db);
    return 1;
}

int send_msg(const char * from_username, const char* to_username, const char * data)
{
    if (!strcmp(from_username, to_username))
        return -1;
    if (!exist(to_username))
        return 0;

    sqlite3 * db;
    int rc = sqlite3_open(db_mess, &db);

    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        perror("[server] Error at sqlite3_open.\n");
        return errno;
    }

    char *create_table_query = "CREATE TABLE IF NOT EXISTS messages ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                        "id_msg INTEGER NOT NULL,"
                        "from_user TEXT NOT NULL,"
                        "to_user TEXT NOT NULL,"
                        "data TEXT NOT NULL,"
                        "view INTEGER NOT NULL);";

    rc = sqlite3_exec(db, create_table_query, 0, 0, 0);

    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Failed to create table: %s\n", sqlite3_errmsg(db));
        perror("[server] Error at sqlite3_create.\n");
        sqlite3_close(db);
        return errno;
    } 
    int viewed = 0;
    for (int i = 0; i < 100; i++)
    {
        if (clients[i] != NULL && !strcmp(clients[i]->username, to_username))
        {
            viewed = 1;
            break;
        }
    }
    int id_msg_insert = get_id_msg(from_username, to_username) + 1;
    char insert_query[NMAX];
    sprintf(insert_query, "INSERT INTO messages (id_msg, from_user, to_user, data, view) VALUES ('%d', '%s', '%s', '%s', '%d');", id_msg_insert, from_username, to_username, data, viewed);
    rc = sqlite3_exec(db, insert_query, 0, 0, 0);
    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Failed to insert data: %s\n", sqlite3_errmsg(db));
        perror("[server] Error at sqlite3_exec.\n");
        sqlite3_close(db);
        return errno;
    }
    char formatted_msg[1024];
    bzero(formatted_msg, 1024);
    sprintf(formatted_msg, "[id_msg : %d] [user: %s] : %s", id_msg_insert, from_username, data);
    strcat(formatted_msg, "\n");
    for (int i = 0; i < 100; i++)
    {
        if (clients[i] != NULL && !strcmp(clients[i]->username, to_username))
        {
            if(send(clients[i]->cl, formatted_msg, strlen(formatted_msg), 0) == -1)
            {
                perror("Error at write(27)\n");
                break;
            }
        }
    }
    sqlite3_close(db);
    return 1;
}

int reply(const char* from_username, const char* to_username, const char* id_msg_reply, const char * data)
{
    if (!strcmp(from_username, to_username))
        return -2;
    if (!exist(to_username))
        return -1;
    int get_max_id = get_id_msg(to_username, from_username);
    int id_int = atoi(id_msg_reply);
    if (id_int < 0 || id_int > get_max_id)
        return 0;

    char new_data[NMAX];
    bzero(new_data, NMAX);
    sprintf(new_data, "Reply -> [id_msg : %s] [user: %s] <- : %s", id_msg_reply, to_username, data);
    int ok = send_msg(from_username, to_username, new_data);
    return ok;
}

int view_received_messages(int cl, int idThread, const char* username)
{
    sqlite3 * db;
    int rc = sqlite3_open(db_mess, &db);

    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        perror("[server] Error at sqlite3_open.\n");
        return errno;
    }

    char query[NMAX];
    bzero(query, NMAX);
    snprintf(query, sizeof(query), "SELECT id_msg, from_user, data FROM messages WHERE to_user = ? AND view = 0");

    sqlite3_stmt *stmt;

    rc = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    int ok = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) 
    {
        int id_msg = sqlite3_column_int(stmt, 0);
        const char *from_username = (const char *)sqlite3_column_text(stmt, 1);
        const char *data = (const char *)sqlite3_column_text(stmt, 2);
        char formatted_answer[NMAX];
        bzero(formatted_answer, NMAX);
        sprintf(formatted_answer, "[id_msg : %d] [user: %s] : %s", id_msg, from_username, data);
        strncpy(message_for_client, formatted_answer, 1024);
        strcat(message_for_client, "\n");
        if (write(cl, message_for_client, sizeof(message_for_client)) <= 0) // this part is so that we will read all the data base even though if greater than 1024
        {
            printf("[Thread %d] ", idThread);
            perror("[Thread] Error at write(22) to the client.\n");
        }
        snprintf(query, sizeof(query), "UPDATE messages SET view = 1 WHERE id_msg = %d", id_msg);
        rc = sqlite3_exec(db, query, 0, 0, 0);
        if (rc != SQLITE_OK) 
        {
            fprintf(stderr, "Failed to update view column: %s\n", sqlite3_errmsg(db));
            ok = -1;
            break;
        }
        ok = 1;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

int view_history(int cl, int idThread, const char* from_username, const char* to_username)
{
    if (!strcmp(from_username, to_username))
        return -1;
    if (!exist(to_username))
        return 0;
    sqlite3 * db;
    int rc = sqlite3_open(db_mess, &db);

    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        perror("[server] Error at sqlite3_open.\n");
        return errno;
    }

    char query[NMAX];
    bzero(query, NMAX);
    snprintf(query, sizeof(query), "SELECT id_msg, from_user, to_user, data FROM messages "
             "WHERE (from_user = ? AND to_user = ?) OR (from_user = ? AND to_user = ?)");

    sqlite3_stmt *stmt;

    rc = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    sqlite3_bind_text(stmt, 1, from_username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, to_username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, to_username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, from_username, -1, SQLITE_STATIC);

    int ok = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) 
    {
        int id_msg = sqlite3_column_int(stmt, 0);
        const char *from_user = (const char *)sqlite3_column_text(stmt, 1);
        const char *data = (const char *)sqlite3_column_text(stmt, 3);

        char formatted_answer[NMAX];
        bzero(formatted_answer, NMAX);
        snprintf(formatted_answer, NMAX, "[id_msg : %d] [user: %s] : %s", id_msg, from_user, data);
        char aux[NMAX];
        bzero(aux, NMAX);
        strcat(aux, formatted_answer);
        strcat(aux, "\n");
        strcpy(message_for_client, aux);
        if (write(cl, message_for_client, sizeof(message_for_client)) <= 0) // this part is so that we will read all the data base even though if greater than 1024
        {
            printf("[Thread %d] ", idThread);
            perror("[Thread] Error at write(22) to the client.\n");
        }
        snprintf(query, sizeof(query), "UPDATE messages SET view = 1 WHERE id_msg = %d", id_msg);
        rc = sqlite3_exec(db, query, 0, 0, 0);
        if (rc != SQLITE_OK) 
        {
            fprintf(stderr, "Failed to update view column: %s\n", sqlite3_errmsg(db));
            break;
        }
        ok = 1;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if (ok)
        return 2;
    else return 1;
}

int get_nr_of_unread_msg(const char * username)
{
    sqlite3 * db;
    int  rc = sqlite3_open(db_mess, &db);
    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        perror("[server] Error at sqlite3_open.\n");
    }  

    char *create_table_query = "CREATE TABLE IF NOT EXISTS messages ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                        "id_msg INTEGER NOT NULL,"
                        "from_user TEXT NOT NULL,"
                        "to_user TEXT NOT NULL,"
                        "data TEXT NOT NULL,"
                        "view INTEGER NOT NULL);";

    rc = sqlite3_exec(db, create_table_query, 0, 0, 0);

    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Failed to create table: %s\n", sqlite3_errmsg(db));
        perror("[server] Error at sqlite3_create.\n");
        sqlite3_close(db);
        return errno;
    } 

    char query[NMAX];
    bzero(query, NMAX);
    snprintf(query, sizeof(query), "SELECT COUNT(*) FROM messages WHERE to_user = ? AND view = 0");
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, 0);
    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return rc;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) 
    {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return count;
}

int blacklist_user(const char* username)
{
    if (!strncmp(username, "admin", 6))
        return -1;
    if (!exist(username))
        return 0;
    sqlite3 * db;
    int rc = sqlite3_open(db_accounts, &db);

    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        perror("[server] Error at sqlite3_open.\n");
        return errno;
    }

    char query[NMAX];
    bzero(query, NMAX);
    snprintf(query, sizeof(query), "UPDATE users SET blacklist = 1 WHERE username = '%s';", username);

    rc = sqlite3_exec(db, query, 0, 0, 0);

    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Failed to update data: %s\n", sqlite3_errmsg(db));
        perror("[server] Error at sqlite3_exec.\n");
        sqlite3_close(db);
        return errno;
    }

    sqlite3_close(db);
    return 1;
}

int whitelist_user(const char* username)
{
    if (!strncmp(username, "admin", 6))
        return -1;
    if (!exist(username))
        return 0;
    sqlite3 * db;
    int rc = sqlite3_open(db_accounts, &db);

    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        perror("[server] Error at sqlite3_open.\n");
        return errno;
    }

    char query[NMAX];
    bzero(query, NMAX);
    snprintf(query, sizeof(query), "UPDATE users SET blacklist = 0 WHERE username = '%s';", username);

    rc = sqlite3_exec(db, query, 0, 0, 0);

    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Failed to update data: %s\n", sqlite3_errmsg(db));
        perror("[server] Error at sqlite3_exec.\n");
        sqlite3_close(db);
        return errno;
    }

    sqlite3_close(db);
    return 1;
}

int main ()
{
    insert_admin();
    struct sockaddr_in server;
    struct sockaddr_in from;	
    int sd; 
    pthread_t th[100];
	int id = 0;
    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server] Error at socket().\n");
        return errno;
    }
    int on = 1;
    setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
    
    bzero (&server, sizeof (server));
    bzero (&from, sizeof (from));
  
    server.sin_family = AF_INET;	
    server.sin_addr.s_addr = htonl (INADDR_ANY);
    server.sin_port = htons (PORT);
  
    if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
        perror("[server] Error at bind().\n");
        return errno;
    }

    if (listen (sd, 2) == -1)
    {
        perror("[server] Error at listen().\n");
        return errno;
    }
    while (1)
    {
        int client;
        thData * td;     
        socklen_t length = sizeof(from);

        printf ("[server] Waiting at the port %d...\n",PORT);
        fflush (stdout);

        if ((client = accept(sd, (struct sockaddr *) &from, &length)) < 0)
        {
            perror("[server] Error at accept().\n");
            continue;
        }

        td=(struct thData*)malloc(sizeof(struct thData));	
        td->idThread=id++;
        td->cl=client;

	    pthread_create(&th[id], NULL, &treat, td);	      
				
	}  
};			

static void *treat(void * arg)
{		
	struct thData *tdL; 
	tdL= (struct thData*)arg;	
	printf ("[thread]- %d - Waiting the command...\n", tdL->idThread);
	fflush (stdout);		 
	pthread_detach(pthread_self());		
	answer((struct thData*)arg);
	close ((intptr_t)arg);
	return(NULL);		
};

void answer(void *arg)
{
	struct thData *tdL; 
	tdL = (struct thData*)arg;
    int is_logged = 0, is_admin = 0;
    while (1)
    {
        bzero(message_from_client, NMAX);
        if (read(tdL->cl, message_from_client, sizeof(message_from_client)) <= 0)
        {
            printf("[Thread %d] ",tdL->idThread);
		    perror("[Thread] Error at read(1) to the client.\n");
        }
        printf("The command received: %s\n", message_from_client);
        
        if (!strncmp(message_from_client, "Login", 5)) // login using the email and the passowrd
        {
            if (!is_logged) 
            {
                char email[NMAX], password[NMAX];
                bzero(email, NMAX);
                bzero(password, NMAX);
                strncpy(message_for_client, "Insert email: ", 15);
                if (write(tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror("Error at write(1) to the client.\n");
                }
                if (read(tdL->cl, email, sizeof(email)) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror("Error at read(2) from the client.\n");
                }
                strncpy(message_for_client, "Insert password: ", 18);
                if (write(tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror("Error at write(2) to the client.\n");
                }
                if (read(tdL->cl, password, sizeof(password)) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror("Error at read(3) from the client.\n");
                }
                char *username = malloc(64);
                int result = check_login_credentials(email, password, username);
                if (result == 1)
                {
                    int connected = 0;
                    for (int i = 0; i < 100; i++)
                        if (clients[i] != NULL && !strcmp(clients[i]->username, username))
                            connected = 1;
                    if (connected)
                        strncpy(message_for_client, "You are already connected\n", 27);
                    else 
                    {
                        add_client(tdL);
                        is_logged = 1;
                        if (!strncmp("admin", username, 6))
                            is_admin = 1;
                        strcpy(tdL->username, username);

                        int unread_cnt = get_nr_of_unread_msg(username);
                        char id_c[5];
                        sprintf(id_c, "%d", unread_cnt);
                        strncpy(message_for_client, "You have connected successfully! ", 34);
                        strcat(message_for_client, "You have ");
                        strcat(message_for_client, id_c);
                        strcat(message_for_client, " unread messages!\n");
                        strcat(message_for_client, "Get users\nGet online users\nSend\nReply\nView messages\nHistory\nLogout\nQuit\n");

                        printf("The user %s connected to the server.\n", tdL->username);
                        printf("The user %s is online.\n", tdL->username);
                    }
                }
                else if (result == 2)
                {
                    strncpy(message_for_client, "The account is blacklisted!\n", 29);
                }
                else if (!result)
                {
                    strncpy(message_for_client, "Failed to login. Wrong email or password!\n", 43);
                }
                if (write(tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror ("Error at write(3) to the client.\n");
                }
                free(username);
            }
            else 
            {
                strncpy(message_for_client, "You are already connected with another account\n", 48);
                if (write(tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror ("Error at write(4) to the client.\n");
                }
            }
        }
        else if (!strncmp(message_from_client, "Register", 8)) // register an account providing an username, an email and a password
        {
            if (!is_logged)
            {
                char username[NMAX], email[NMAX], password[NMAX];
                bzero(username, NMAX);
                bzero(email, NMAX);
                bzero(password, NMAX);
                strncpy(message_for_client, "Insert username: ", 18);
                if (write(tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror ("Error at write(5) to the client.\n");
                }
                if (read(tdL->cl, username, sizeof(username)) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror("Error at read(4) from the client.\n");
                }
                strncpy(message_for_client, "Insert email: ", 15);
                if (write(tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror("Error at write(6) to the client.\n");
                }
                if (read(tdL->cl, email, sizeof(email)) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror("Error at read(5) from the client.\n");
                }
                strncpy(message_for_client, "Insert password: ", 18);
                if (write(tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror("Error at write(7) to the client.\n");
                }
                if (read(tdL->cl, password, sizeof(password)) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror("Error at read(6) from the client.\n");
                }  
                int ok = register_account(username, email, password); 
                switch (ok)
                {
                    case -3: 
                        strncpy(message_for_client, "Invalid username \n", 19);
                        break;

                    case -2: 
                        strncpy(message_for_client, "Invalid email\n", 15);
                        break;

                    case -1: 
                        strncpy(message_for_client, "Invalid password\n", 18);
                        break;                  

                    case 0: 
                        strncpy(message_for_client, "The username is already being used by another account\n", 55);
                        break;

                    case 1: 
                        strncpy(message_for_client, "The email is already being used by another account\n", 52);
                        break;

                    case 2:  
                        strncpy(tdL->username, username, 64);
                        strncpy(message_for_client, "The account has been created succesfully!\n", 43);

                        printf("The account with the username %s and email %s has been created.\n", tdL->username, email);
                        break;
                    
                    default:
                        break;
                }
                if (write(tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror("Error at write(8) to the client.\n");
                }
            }
            else 
            {
                strncpy(message_for_client, "Logout from the current account if you want to create a new one.\n", 66);
                if (write(tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror("Error at write(9) to the client.\n");
                }
            }
        }
        else if (!strncmp(message_from_client,"Get users", 9)) /// get the all existing users from the database
        {
            if (!is_logged)
            {
                strncpy(message_for_client, "You have to be connected if you want to use this command\nLogin\nRegister\nQuit\n", 78);
                if (write (tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ",tdL->idThread);
                    perror("[Thread] Error at write(10) to the client.\n");
                }
            }
            else 
            {
                get_users(); /// return all the users from the database
                if (write (tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ",tdL->idThread);
                    perror("[Thread] Error at write(11) to the client.\n");
                }
            }
        }
        else if (!strncmp(message_from_client, "Get online users", 16)) // get all online users
        {
            if (!is_logged)
            {
                strncpy(message_for_client, "You have to be connected if you want to use this command\nLogin\nRegister\nQuit\n", 78);
                if (write (tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ",tdL->idThread);
                    perror("[Thread] Error at write(12) to the client.\n");
                }
            }
            else 
            {
                get_online_users();
                if (write (tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ",tdL->idThread);
                    perror("[Thread] Error at write(13) to the client.\n");
                }
            }
        }
        else if (!strncmp(message_from_client, "Send", 4)) // send a message to a user by providing a username and the message
        {
            if (!is_logged)
            {
                strncpy(message_for_client, "You have to be connected if you want to use this command\nLogin\nRegister\nQuit\n", 78);
                if (write (tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ",tdL->idThread);
                    perror("[Thread] Error at write(14) to the client.\n");
                }
            }
            else 
            {
                char username_from_client[NMAX], data[NMAX];
                bzero(username_from_client, NMAX);
                bzero(data, NMAX);
                strncpy(message_for_client, "Username: ", 11);
                if (write (tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ",tdL->idThread);
                    perror("[Thread] Error at write(15) to the client.\n");
                }
                if (read(tdL->cl, username_from_client, sizeof(username_from_client)) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror("Error at read(7) from the client.\n");
                }
                strncpy(message_for_client, "Text: ", 7);
                if (write (tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ",tdL->idThread);
                    perror("[Thread] Error at write(16) to the client.\n");
                }
                if (read(tdL->cl, data, sizeof(data)) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror("Error at read(8) from the client.\n");
                }
                int ok = send_msg(tdL->username, username_from_client, data);
                switch (ok)
                {
                    case -1:
                        strncpy(message_for_client, "You can't send message to yourself\n", 36);
                        break;
                    case 0:
                        strncpy(message_for_client, "This user does not exist\n", 26);
                        break;
                    case 1:
                        strncpy(message_for_client, "Message sent!\n", 15);
                        break;

                    default:
                        break;
                }
                if (write(tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ",tdL->idThread);
                    perror("[Thread] Error at write(17) to the client.\n");
                }
            }
        }
        else if (!strncmp(message_from_client, "Reply", 6)) // reply to a specific message from a user by providing an username, id of the message and the reply's message
        {
            if (!is_logged)
            {
                strncpy(message_for_client, "You have to be connected if you want to use this command\nLogin\nRegister\nQuit\n", 78);
                if (write (tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ",tdL->idThread);
                    perror("[Thread] Error at write(18) to the client.\n");
                }
            }
            else 
            {
                char username_from_client[NMAX], id_msg_reply[NMAX], data[NMAX];
                bzero(username_from_client, NMAX);
                bzero(id_msg_reply, NMAX);
                bzero(data, NMAX);
                strncpy(message_for_client, "Username: ", 11);
                if (write (tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ", tdL->idThread);
                    perror("[Thread] Error at write(15) to the client.\n");
                }
                if (read(tdL->cl, username_from_client, sizeof(username_from_client)) <= 0)
                {
                    printf("[Thread %d]\n", tdL->idThread);
                    perror("Error at read(7) from the client.\n");
                }
                strncpy(message_for_client, "Message ID: ", 13);
                if (write (tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ", tdL->idThread);
                    perror("[Thread] Error at write(15) to the client.\n");
                }
                if (read(tdL->cl, id_msg_reply, sizeof(id_msg_reply)) <= 0)
                {
                    printf("[Thread %d]\n",tdL->idThread);
                    perror("Error at read(7) from the client.\n");
                }
                strncpy(message_for_client, "Text: ", 7);
                if (write (tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ", tdL->idThread);
                    perror("[Thread] Error at write(16) to the client.\n");
                }
                if (read(tdL->cl, data, sizeof(data)) <= 0)
                {
                    printf("[Thread %d]\n", tdL->idThread);
                    perror("Error at read(8) from the client.\n");
                }
                int ok = reply(tdL->username, username_from_client, id_msg_reply, data);
                switch (ok)
                {
                    case -2:
                        strncpy(message_for_client, "You can't reply to yourself\n", 29);
                        break;
                    case -1:
                        strncpy(message_for_client,"This user does not exist\n", 26);
                        break;
                    case 0:
                        strncpy(message_for_client,"Invalid message ID!\n", 21);
                        break;
                    case 1:
                        strncpy(message_for_client,"Reply sent!\n", 13);
                    default:
                        break;
                }
                if (write(tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ", tdL->idThread);
                    perror("[Thread] Error at write(17) to the client.\n");
                }
            }
        }
        else if (!strncmp(message_from_client, "View messages", 13)) // view unread messages from all users
        {
            if (!is_logged)
            {
                strncpy(message_for_client, "You have to be connected if you want to use this command\nLogin\nRegister\nQuit\n", 78);
                if (write (tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ", tdL->idThread);
                    perror("[Thread] Error at write(18) to the client.\n");
                }
            }
            else 
            {
                int ok = view_received_messages(tdL->cl, tdL->idThread, tdL->username);
                if (!ok) {
                    strncpy(message_for_client, "Empty inbox\n", 13);
                    if (write(tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                    {
                        printf("[Thread %d] ", tdL->idThread);
                        perror("[Thread] Error at write(19) to the client.\n");
                    }
                }
            }
        }
        else if (!strncmp(message_from_client, "History", 7)) // view all messages from a conversation with a specific user by providing the username
        {
            if (!is_logged)
            {
                strncpy(message_for_client, "You have to be connected if you want to use this command\nLogin\nRegister\nQuit\n", 78);
                if (write(tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ", tdL->idThread);
                    perror("[Thread] Error at write(20) to the client.\n");
                }
            }
            else 
            {
                char user_his[NMAX];
                strncpy(message_for_client, "Username: ", 11);
                if (write (tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ", tdL->idThread);
                    perror("[Thread] Error at write(21) to the client.\n");
                }
                if (read(tdL->cl, user_his, sizeof(user_his)) <= 0)
                {
                    printf("[Thread %d]\n", tdL->idThread);
                    perror("Error at read(9) from the client.\n");
                }
                int ok = view_history(tdL->cl, tdL->idThread, tdL->username, user_his);
                switch (ok)
                {
                    case -1:
                        strncpy(message_for_client, "You can't view history of yourself\n", 36);
                        break;
                    case 0:
                        strncpy(message_for_client,"This user does not exist\n", 26);
                        break;
                    case 1:
                        strncpy(message_for_client, "Empty history\n", 15);
                        break;
                    default:
                        break;
                }
                if (ok < 2)
                    if (write(tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                    {
                        printf("[Thread %d] ", tdL->idThread);
                        perror("[Thread] Error at write(22) to the client.\n");
                    }
            }
        }
        else if (!strncmp(message_from_client, "Logout", 6)) // Logout from the account 
        {
            if (!is_logged)
                strncpy(message_for_client, "You are not connected!\nLogin\nRegister\nQuit\n", 44);
            else 
            {
                printf("The user %s is offline!\n", tdL->username);
                is_logged = 0;
                is_admin = 0;
                remove_client(tdL);
                strncpy(message_for_client, "You have successfully logout!\nLogin\nRegister\nQuit\n", 51);
            }
            if (write (tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
            {
                printf("[Thread %d] ", tdL->idThread);
                perror("[Thread] Error at write(23) to the client.\n");
            }
        }
        else if (!strncmp(message_from_client, "Blacklist", 9))
        {
            if (!is_logged)
            {
                strncpy(message_for_client, "You are not connected!\nLogin\nRegister\nQuit\n", 44);
                if (write(tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ", tdL->idThread);
                    perror("[Thread] Error at write(24) to the client.\n");
                }
            }
            else if (is_logged && !is_admin)
            {
                strncpy(message_for_client, "You are not an admin!\nLogin\nRegister\nQuit\n", 44);
                if (write(tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ", tdL->idThread);
                    perror("[Thread] Error at write(25) to the client.\n");
                }
            }
            else if (is_logged && is_admin)
            {
                char username_from_client[NMAX];
                strncpy(message_for_client, "Username: ", 11);
                if (write (tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ", tdL->idThread);
                    perror("[Thread] Error at write(26) to the client.\n");
                }
                if (read(tdL->cl, username_from_client, sizeof(username_from_client)) <= 0)
                {
                    printf("[Thread %d]\n", tdL->idThread);
                    perror("Error at read(10) from the client.\n");
                }
                int ok = blacklist_user(username_from_client);
                switch (ok)
                {
                    case -1:
                        strncpy(message_for_client, "You can't blacklist the admin\n", 31);
                        break;
                    case 0:
                        strncpy(message_for_client, "This user does not exist\n", 26);
                        break;
                    case 1:
                        strncpy(message_for_client, "Blacklist successful\n", 22);
                        break;
                    default:
                        break;
                }
                if (write(tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ", tdL->idThread);
                    perror("[Thread] Error at write(27) to the client.\n");
                }
            }
        }
        else if (!strncmp(message_from_client, "Whitelist", 9))
        {
            if (!is_logged)
            {
                strncpy(message_for_client, "You are not connected!\nLogin\nRegister\nQuit\n", 44);
                if (write(tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ", tdL->idThread);
                    perror("[Thread] Error at write(28) to the client.\n");
                }
            }
            else if (is_logged && !is_admin)
            {
                strncpy(message_for_client, "You are not an admin!\nLogin\nRegister\nQuit\n", 44);
                if (write(tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ", tdL->idThread);
                    perror("[Thread] Error at write(29) to the client.\n");
                }
            }
            else if (is_logged && is_admin)
            {
                char username_from_client[NMAX];
                strncpy(message_for_client, "Username: ", 11);
                if (write (tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ", tdL->idThread);
                    perror("[Thread] Error at write(30) to the client.\n");
                }
                if (read(tdL->cl, username_from_client, sizeof(username_from_client)) <= 0)
                {
                    printf("[Thread %d]\n", tdL->idThread);
                    perror("Error at read(11) from the client.\n");
                }
                int ok = whitelist_user(username_from_client);
                switch (ok)
                {
                    case -1:
                        strncpy(message_for_client, "You can't whitelist the admin\n", 31);
                        break;
                    case 0:
                        strncpy(message_for_client, "This user does not exist\n", 26);
                        break;
                    case 1:
                        strncpy(message_for_client, "Whitelist successful\n", 22);
                        break;
                    default:
                        break;
                }
                if (write(tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
                {
                    printf("[Thread %d] ", tdL->idThread);
                    perror("[Thread] Error at write(31) to the client.\n");
                }
            }
        }
        else if (!strncmp(message_from_client, "Help", 4)) // diplays all the accepted commands
        {
            strncpy(message_for_client, "\n- Login\n- Register\n- Send\n- Reply\n- Get users\n- Get online users\n- History\n- View messages\n- Blacklist (admin only)\n- Whitelist (admin only)\n- Logout\n- Quit\n\n", 160);
            if (write (tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
            {
                printf("[Thread %d] ", tdL->idThread);
                perror("[Thread] Error at write(32) to the client.\n");
            }
        }
        else if (!strncmp(message_from_client, "Quit", 4)) // Quit the client
        {
            if (is_logged)
                printf("The user %s is offline.\n", tdL->username);
            else printf("The client has been closed\n");
            is_logged = 0;
            is_admin = 0;
            remove_client(tdL);
            break;
        }
        else // case for the command that is not ok
        {
            strncpy(message_for_client, "This command doesn't exit!\n", 28);
            if (write (tdL->cl, message_for_client, sizeof(message_for_client)) <= 0)
            {
                printf("[Thread %d] ", tdL->idThread);
                perror("[Thread] Error at write(33) to the client.\n");
            }
            else printf("[Thread %d] The message has been sent successfully.\n", tdL->idThread);	
        }
        bzero(message_for_client, NMAX);
    }
}