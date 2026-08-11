#include "Application.h"

/**
 * @file main.cpp
 * @brief Entry point for the TagIt application.
 *
 * TagIt is an intelligent cross-platform music library manager.
 * See README.md for more information.
 */
int main(int argc, char *argv[])
{
    tagit::Application app(argc, argv);
    return app.run();
}

