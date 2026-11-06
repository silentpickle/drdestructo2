#include "stdafx.h"
#include "Random.h"

#ifdef __EMSCRIPTEN__
#include <random>

namespace
{
std::mt19937 & GetGen()
{
    static std::mt19937 gen(time(nullptr));
    return gen;
}
}

int RndInt(int f, int t)
{
    return std::uniform_int_distribution<int>(f, t)(GetGen());
}

float RndFloat(float f, float t)
{
    return std::uniform_real_distribution<float>(f, t)(GetGen());
}
#else
boost::mt19937 & GetGen()
{
	static boost::mt19937 gen(time(0));
	return gen;
}

int RndInt( int f, int t )
{
	boost::uniform_int<> dist(f, t);
	return dist( GetGen() );
}

float RndFloat( float f, float t )
{
	boost::uniform_real<> dist(f, t);
	return dist( GetGen() );
}
#endif
