#ifndef __SERVER_HPP__
#define __SERVER_HPP__

class DataBase;

void backend_server(DataBase& db);
void start_connection_server(DataBase& db);

#endif
