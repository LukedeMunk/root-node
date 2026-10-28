/*
 * File:   testprogram.c
 * Author: admin
 *
 * To test superuser login. DELETE WHEN FINISHING DEVELOPMENT!!!
 */
#include <stdio.h>
#include <string.h>

/******************************************************************************/
/*!                                                                             
  @brief  Logs given user name in as superuser and logs out again.              
*/                                                                              
/******************************************************************************/
int main(int argc, char** argv) {
    char username[10];
    strcpy(username, argv[1]);
    login(*username);
    logout();
    
    return 0;
}