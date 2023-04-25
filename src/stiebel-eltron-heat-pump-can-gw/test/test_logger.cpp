
#include "../Logger.h"

int main() {
    Logger logger;
    logger.log("test", "message 1");
    logger.output([](const char* entry){ printf("%s", entry); });
    return 0;
}