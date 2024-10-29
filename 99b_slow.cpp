// C++ version of 99 Bottles of beer
// programmer: Tim Robinson timtroyr@ionet.net
// Corrections by Johannes Tevessen

#include <iostream>
#include <cstdint>
using namespace std;

int main()
    {
    uint32_t bottles = 0xFFFFFF;
    while ( bottles > 0 )
        {
        cout << bottles << " bottle(s) of beer on the wall," << endl;
        cout << bottles << " bottle(s) of beer." << endl;
        cout << "Take one down, pass it around," << endl;
        cout << --bottles << " bottle(s) of beer on the wall." << endl;
        }
    return 0;
    }
