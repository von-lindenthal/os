#include "auth.h"
#include "string.h"

static char current_user[24];
static char password[24];
static int logged_in;

void auth_init(void)
{
    strlcpy(current_user, "guest", sizeof(current_user));
    strlcpy(password, "os", sizeof(password));
    logged_in = 0;
}

const char *auth_user(void)
{
    return logged_in ? current_user : "guest";
}

int auth_is_logged_in(void)
{
    return logged_in;
}

int auth_login(const char *user, const char *pass)
{
    if (!user || !user[0] || strlen(user) >= sizeof(current_user))
        return -1;
    if (!pass)
        pass = "";
    if (strcmp(pass, password) != 0)
        return -2;
    strlcpy(current_user, user, sizeof(current_user));
    logged_in = 1;
    return 0;
}

void auth_logout(void)
{
    logged_in = 0;
    strlcpy(current_user, "guest", sizeof(current_user));
}

int auth_set_pass(const char *pass)
{
    if (!pass || strlen(pass) >= sizeof(password))
        return -1;
    strlcpy(password, pass, sizeof(password));
    return 0;
}
