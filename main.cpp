//Using SDL and standard IO
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <sstream>
#include "ThreadPool.h"


//The timer for provide FPS Count
class Timer
{
private:
    //The clock time when the timer started
    int startTicks;

    //The ticks stored when the timer was paused
    int pausedTicks;

    //The timer status
    bool paused;
    bool started;

public:
    //Initializes variables
    Timer();

    //The various clock actions
    void start();
    void stop();
    void pause();
    void unpause();

    //Gets the timer's time
    int get_ticks();

    //Checks the status of the timer
    bool is_started();
    bool is_paused();
};




//Screen dimension constants
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

//The window we'll be rendering to
SDL_Window* window = NULL;

//The surface contained by the window
SDL_Surface* screenSurface = NULL;

//SDL Renderer
SDL_Renderer* gRenderer = NULL;
//SDL_Texture to Blit Mandelbrot image
SDL_Texture* texture = NULL;

//Thread poo interface (extern)
ThreadPool poolThread;

//Need Exit?
int quit = 0;
//Catch Events
SDL_Event e;

//Array for put each pixel of set
unsigned char pixelArray[SCREEN_WIDTH*SCREEN_HEIGHT*4];

//Keep track of the frame count
int frame = 0;
int CPUCount = 0;
//Timer used to calculate the frames per second
Timer fps;

//Timer used to update the caption
Timer update;

//Mandelbrot set Setup
double MinRe = -2.0;
double MaxRe = 1.0;
double MinIm = -1.2;
double MaxIm = MinIm+(MaxRe-MinRe)*SCREEN_HEIGHT/SCREEN_WIDTH;
double Re_factor = (MaxRe-MinRe)/(SCREEN_WIDTH-1);
double Im_factor = (MaxIm-MinIm)/(SCREEN_HEIGHT-1);
const unsigned MaxIterations = 128;

//Color Palette
unsigned char colorPal[MaxIterations*4];

void draw_madelbrot();

//Task of compute mandelbrot slice
//this is sent to ThreadPool
class SliceComputeTask : Task
{
public:
    double step; // is ScreenHeight / NumberOfCores
    unsigned index; //Actual "Slice"
    int state; //Used for wait all threads done

    virtual void *DoWork()
    {
        state = Running;

        //Offset of slice
        double y1 = step*index;
        for(unsigned y=y1; y<y1+step; ++y)
        {
            double c_im = MaxIm - y*Im_factor;
            for(unsigned x=0; x<SCREEN_WIDTH; ++x)
            {
                double c_re = MinRe + x*Re_factor;

                double Z_re = c_re, Z_im = c_im;
                bool isInside = true;
                //offset of pixelBuffer
                const unsigned offset = ( SCREEN_WIDTH * 4 * y ) + x * 4;
                //offset of Color Palette
                unsigned pOffset = 0;
                unsigned n;

                for(n=0; n<MaxIterations; ++n)
                {
                    double Z_re2 = Z_re*Z_re, Z_im2 = Z_im*Z_im;


                    if(Z_re2 + Z_im2 > 4)
                    {
                        isInside = false;
                        break;
                    }
                    else
                    {
                        //Draw Pixel According with color table
                        pixelArray[offset] = colorPal[n];
                        pixelArray[offset + 1] = colorPal[n+1];
                        pixelArray[offset + 2] = colorPal[n+2];
                        pixelArray[offset + 3] = SDL_ALPHA_OPAQUE;


                    }
                    Z_im = 2*Z_re*Z_im + c_im;
                    Z_re = Z_re2 - Z_im2 + c_re;
                }
                if(isInside)
                {
                    //Draw pixel
                    pixelArray[offset] = 0;
                    pixelArray[offset + 1] = 255;
                    pixelArray[offset + 2] = 255;
                    pixelArray[offset + 3] = SDL_ALPHA_OPAQUE;

                }
            }
        }
        //Task finished
        state = Finished;

    };


};

//Pointer to all task
//don't alloc task on each thread
SliceComputeTask *sliceTasks = NULL;


int main( int argc, char* args[] )
{

    //Initialize thread Pool

    //Get CPU Count
    CPUCount = SDL_GetCPUCount();

    //initialize Threads
    poolThread.init(CPUCount);

    //allocate task's in memory
    sliceTasks = new SliceComputeTask[CPUCount];

    //generate color Palette
    for (int k = 0; k < MaxIterations; k++)
    {
        double s = 6.2831 * k / MaxIterations;


        double r = 0.6 + 0.4 * cos(s + 0.9);
        double g = 0.6 + 0.4 * cos(s + 0.3);
        double b = 0.6 + 0.4 * cos(s + 0.2);
        double a = SDL_ALPHA_OPAQUE;

        if (r > 1)
            r = 1;
        if (g > 1)
            g = 1;
        if (b > 1)
            b = 1;

        colorPal[k + 0] = k*20;
        colorPal[k + 1] = k;
        colorPal[k + 2] = 0;
        colorPal[k + 3] = SDL_ALPHA_OPAQUE;
    }


    //Initialize SDL
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
    {
        printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
    }
    else
    {
        //Create window
        window = SDL_CreateWindow( "Gravidade", 100, 100, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN );
        if( window == NULL )
        {
            printf( "Window could not be created! SDL_Error: %s\n", SDL_GetError() );
        }
        else
        {
            //Get window surface
            screenSurface = SDL_GetWindowSurface( window );
            gRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED );
            if( gRenderer == NULL )
            {
                printf( "Renderer could not be created! SDL Error: %s\n", SDL_GetError() );
            }
            else
            {
                //Initialize renderer color
                SDL_SetRenderDrawColor( gRenderer, 0x00, 0x00, 0x00, 0xFF );
            }

            //Create texture for blit Mandelbrot to screen
            SDL_Texture* texture = SDL_CreateTexture
                                   (
                                       gRenderer,
                                       SDL_PIXELFORMAT_ARGB8888,
                                       SDL_TEXTUREACCESS_STREAMING,
                                       SCREEN_WIDTH, SCREEN_HEIGHT
                                   );

            //Start the update timer
            update.start();

            //Start the frame timer
            fps.start();
            while(quit == 0)
            {
                while( SDL_PollEvent( &e ) != 0 )
                {
                    //User requests quit
                    if( e.type == SDL_QUIT )
                    {
                        quit = 1;
                    }
                }


                //Draw Mandelbrod
                draw_madelbrot();

                //Update texture
                SDL_UpdateTexture
                (
                    texture,
                    NULL,
                    &pixelArray[0],
                    SCREEN_WIDTH * 4
                );
                //Copy to screen
                SDL_RenderCopy( gRenderer, texture, NULL, NULL );
                //Update screen
                SDL_RenderPresent( gRenderer );

                frame++;
                if(  fps.get_ticks() > 1000 )
                {
                    //Sleep the remaining frame time
                    std::stringstream caption;

                    //Calculate the frames per second and create the string
                    caption << "FPS: " << frame / ( fps.get_ticks() / 1000.f );

                    //Reset the caption
                    SDL_SetWindowTitle(window,caption.str().c_str());
                    //Restart the update timer
                    update.start();
                }
            }


        }
    }
    //Destroy window
    SDL_DestroyWindow( window );
    SDL_DestroyTexture(texture);
    poolThread.destroy();
    delete sliceTasks;

//Quit SDL subsystems
    SDL_Quit();

    return 0;
}



//mandelbrot main function
void draw_madelbrot()
{
    //calculeta slice size
    double step = SCREEN_HEIGHT / CPUCount;

    //foreach on CPU
    for (int i = 0; i < CPUCount; i++)
    {

        //Setup task
        sliceTasks[i].step = step;
        sliceTasks[i].index = i;
        sliceTasks[i].state = Initialized;

        //Queque task
        poolThread.addWork((Task*)&sliceTasks[i]);


    }

    //wait all task finish
    int k = 0;
    bool frameDone = false;
    while(frameDone == false)
    {
        if (k+1 == CPUCount)
            frameDone = true;

        if (sliceTasks[k].state == Finished)
            k++;

    }

}

Timer::Timer()
{
    //Initialize the variables
    startTicks = 0;
    pausedTicks = 0;
    paused = false;
    started = false;
}

void Timer::start()
{
    //Start the timer
    started = true;

    //Unpause the timer
    paused = false;

    //Get the current clock time
    startTicks = SDL_GetTicks();
}

void Timer::stop()
{
    //Stop the timer
    started = false;

    //Unpause the timer
    paused = false;
}

void Timer::pause()
{
    //If the timer is running and isn't already paused
    if( ( started == true ) && ( paused == false ) )
    {
        //Pause the timer
        paused = true;

        //Calculate the paused ticks
        pausedTicks = SDL_GetTicks() - startTicks;
    }
}

void Timer::unpause()
{
    //If the timer is paused
    if( paused == true )
    {
        //Unpause the timer
        paused = false;

        //Reset the starting ticks
        startTicks = SDL_GetTicks() - pausedTicks;

        //Reset the paused ticks
        pausedTicks = 0;
    }
}

int Timer::get_ticks()
{
    //If the timer is running
    if( started == true )
    {
        //If the timer is paused
        if( paused == true )
        {
            //Return the number of ticks when the timer was paused
            return pausedTicks;
        }
        else
        {
            //Return the current time minus the start time
            return SDL_GetTicks() - startTicks;
        }
    }

    //If the timer isn't running
    return 0;
}

bool Timer::is_started()
{
    return started;
}

bool Timer::is_paused()
{
    return paused;
}

