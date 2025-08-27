#include "App.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int main(int argc, char** argv) {

	if (argc > 1)
	{
		int seed;
		try {
			seed = std::stoi(argv[1]);
		} catch (std::exception &e) {
			std::cout << e.what() << ": please enter a number for world seed." << std::endl;
			exit(1);
		}
		
		App app(seed);
		app.run();

	} else {
		App app;
		app.run();
	}

	return 0;
}
