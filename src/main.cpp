#include <iostream>
#include <chrono>
#include <thread>
#include <string>

class Player
{
public:
    int c_health;
    std::string c_name;
};

int main()
{
    // TODO: YES!
    Player PlayerCharacter;
    PlayerCharacter.c_health = {100};
    PlayerCharacter.c_name = "Dih";
    std::cout << PlayerCharacter.c_health << PlayerCharacter.c_name;
    int hold;
    std::cin >> hold;
    return 0;
}
