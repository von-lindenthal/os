#ifndef AUTH_H
#define AUTH_H

void auth_init(void);
const char *auth_user(void);
int auth_is_logged_in(void);
int auth_login(const char *user, const char *pass);
void auth_logout(void);
int auth_set_pass(const char *pass);

#endif
