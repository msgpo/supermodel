/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2017 Bart Trzynadlowski, Nik Henson, Ian Curtis
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free 
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/
 
/*
 * Main.cpp
 * 
 * Main program driver for the SDL port.
 *
 * To Do Before Next Release
 * -------------------------
 * - Thoroughly test config system (do overrides work as expected? XInput
 *   force settings?)
 * - Make sure fragment and vertex shaders are configurable for 3D (and 2D?)
 * - Remove all occurrences of "using namespace std" from Nik's code.
 * - Standardize variable naming (recently introduced vars_like_this should be
 *   converted back to varsLikeThis).
 * - Update save state file revision (strings > 1024 chars are now supported).
 * - Fix BlockFile.cpp to use fstream! 
 * - Check to make sure save states use explicitly-sized types for 32/64-bit
 *   compatibility (i.e., size_t, int, etc. not allowed).
 * - Make sure quitting while paused works.
 * - Add UI keys for balance setting? 
 * - 5.1 audio support?
 * - Stretch video option
 *
 * Compile-Time Options
 * --------------------
 * - SUPERMODEL_WIN32: Define this if compiling on Windows.
 * - SUPERMODEL_OSX: Define this if compiling on Mac OS X.
 * - SUPERMODEL_DEBUGGER: Enable the debugger.
 * - DEBUG: Debug mode (use with caution, produces large logs of game behavior)
 */

#include <new>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <memory>
#include <vector>
#include <algorithm>
#include "Pkgs/glew.h"

#include "Supermodel.h"
#include "Util/Format.h"
#include "Util/NewConfig.h"
#include "Util/ConfigBuilders.h"
#include "GameLoader.h"
#include "SDLInputSystem.h"
#ifdef SUPERMODEL_WIN32
#include "DirectInputSystem.h"
#include "WinOutputs.h"
#endif

#include <iostream>

// Log file names
#define DEBUG_LOG_FILE  "debug.log"
#define ERROR_LOG_FILE  "error.log"


/******************************************************************************
 Global Run-time Config
******************************************************************************/

static Util::Config::Node s_runtime_config("Global");


/******************************************************************************
 Display Management
******************************************************************************/

/*
 * Position and size of rectangular region within OpenGL display to render to.
 * Unlike the config tree, these end up containing the actual resolution (and
 * computed offsets within the viewport) that will be rendered based on what
 * was obtained from SDL.
 */
static unsigned  xOffset, yOffset;      // offset of renderer output within OpenGL viewport
static unsigned  xRes, yRes;            // renderer output resolution (can be smaller than GL viewport)
static unsigned  totalXRes, totalYRes;  // total resolution (the whole GL viewport)

static bool SetGLGeometry(unsigned *xOffsetPtr, unsigned *yOffsetPtr, unsigned *xResPtr, unsigned *yResPtr, unsigned *totalXResPtr, unsigned *totalYResPtr, bool keepAspectRatio)
{
  // What resolution did we actually get?
  const SDL_VideoInfo *VideoInfo = SDL_GetVideoInfo();
  *totalXResPtr = VideoInfo->current_w;
  *totalYResPtr = VideoInfo->current_h;
  
  // If required, fix the aspect ratio of the resolution that the user passed to match Model 3 ratio
  float xRes = float(*xResPtr);
  float yRes = float(*yResPtr);
  if (keepAspectRatio)
  {
    float model3Ratio = 496.0f/384.0f;
    if (yRes < (xRes/model3Ratio))
      xRes = yRes*model3Ratio;
    if (xRes < (yRes*model3Ratio))
      yRes = xRes/model3Ratio;
  }
  
  // Center the visible area 
  *xOffsetPtr = (*xResPtr - (unsigned) xRes)/2;
  *yOffsetPtr = (*yResPtr - (unsigned) yRes)/2;
  
  // If the desired resolution is smaller than what we got, re-center again
  if (int(*xResPtr) < VideoInfo->current_w)
    *xOffsetPtr += (VideoInfo->current_w - *xResPtr)/2;
  if (int(*yResPtr) < VideoInfo->current_h)
    *yOffsetPtr += (VideoInfo->current_h - *yResPtr)/2;
  
  // OpenGL initialization
  glViewport(0,0,*xResPtr,*yResPtr);
  glClearColor(0.0,0.0,0.0,0.0);
  glClearDepth(1.0);
  glDepthFunc(GL_LESS);
  glEnable(GL_DEPTH_TEST);
  glShadeModel(GL_SMOOTH);
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
  glDisable(GL_CULL_FACE);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(90.0,(GLfloat)xRes/(GLfloat)yRes,0.1,1e5);
  glMatrixMode(GL_MODELVIEW);
  
  // Clear both buffers to ensure a black border
  for (int i = 0; i < 2; i++)
  {
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    SDL_GL_SwapBuffers();
  }
  
  // Write back resolution parameters
  *xResPtr = (unsigned) xRes;
  *yResPtr = (unsigned) yRes;

  UINT32 correction = (UINT32)(((yRes / 384.f) * 2) + 0.5f);

  glEnable(GL_SCISSOR_TEST);
  
  // Scissor box (to clip visible area)
  if (s_runtime_config["WideScreen"].ValueAsDefault<bool>(false))
    glScissor(0, correction, *totalXResPtr, *totalYResPtr - (correction * 2));
  else
    glScissor(*xOffsetPtr + correction, *yOffsetPtr + correction, *xResPtr - (correction * 2), *yResPtr - (correction * 2));
  return OKAY;
}

/*
 * CreateGLScreen():
 *
 * Creates an OpenGL display surface of the requested size. xOffset and yOffset
 * are used to return a display surface offset (for OpenGL viewport commands)
 * because the actual drawing area may need to be adjusted to preserve the 
 * Model 3 aspect ratio. The new resolution will be passed back as well -- both
 * the adjusted viewable area resolution and the total resolution.
 *
 * NOTE: keepAspectRatio should always be true. It has not yet been tested with
 * the wide screen hack.
 */
static bool CreateGLScreen(const char *caption, unsigned *xOffsetPtr, unsigned *yOffsetPtr, unsigned *xResPtr, unsigned *yResPtr, unsigned *totalXResPtr, unsigned *totalYResPtr, bool keepAspectRatio, bool fullScreen)
{
  GLenum err;
  
  // Important GL attributes
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,8)
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);

  // Set video mode
  if (SDL_SetVideoMode(*xResPtr,*yResPtr,0,SDL_OPENGL|(fullScreen?SDL_FULLSCREEN|SDL_HWSURFACE:0)) == NULL)
  {
    ErrorLog("Unable to create an OpenGL display: %s\n", SDL_GetError());
    return FAIL;
  }
    
  // Initialize GLEW, allowing us to use features beyond OpenGL 1.2
  err = glewInit();
  if (GLEW_OK != err)
  {
    ErrorLog("OpenGL initialization failed: %s\n", glewGetErrorString(err));
    return FAIL;
  }
  
  return SetGLGeometry(xOffsetPtr, yOffsetPtr, xResPtr, yResPtr, totalXResPtr, totalYResPtr, keepAspectRatio);
}

static bool ResizeGLScreen(unsigned *xOffsetPtr, unsigned *yOffsetPtr, unsigned *xResPtr, unsigned *yResPtr, unsigned *totalXResPtr, unsigned *totalYResPtr, bool keepAspectRatio, bool fullScreen)
{
  // Set video mode
  if (SDL_SetVideoMode(*xResPtr,*yResPtr,0,SDL_OPENGL|(fullScreen?SDL_FULLSCREEN|SDL_HWSURFACE:0)) == NULL)
  {
    ErrorLog("Unable to create an OpenGL display: %s\n", SDL_GetError());
    return FAIL;
  }
  
  return SetGLGeometry(xOffsetPtr, yOffsetPtr, xResPtr, yResPtr, totalXResPtr, totalYResPtr, keepAspectRatio);
}

/*
 * PrintGLInfo():
 *
 * Queries and prints OpenGL information. A full list of extensions can
 * optionally be printed.
 */
static void PrintGLInfo(bool createScreen, bool infoLog, bool printExtensions)
{
  unsigned xOffset, yOffset, xRes=496, yRes=384, totalXRes, totalYRes;  
  if (createScreen)
  {
    if (OKAY != CreateGLScreen("Supermodel - Querying OpenGL Information...", &xOffset, &yOffset, &xRes, &yRes, &totalXRes, &totalYRes, false, false))
    {
      ErrorLog("Unable to query OpenGL.\n");
      return;
    }
  }
  
  GLint value;
  if (infoLog)  InfoLog("OpenGL information:");
  else             puts("OpenGL information:\n");
  const GLubyte *str = glGetString(GL_VENDOR);
  if (infoLog)  InfoLog("  Vendor                   : %s", str);
  else           printf("  Vendor                   : %s\n", str);
  str = glGetString(GL_RENDERER);
  if (infoLog)  InfoLog("  Renderer                 : %s", str);
  else           printf("  Renderer                 : %s\n", str);
  str = glGetString(GL_VERSION);
  if (infoLog)  InfoLog("  Version                  : %s", str);
  else           printf("  Version                  : %s\n", str);
  str = glGetString(GL_SHADING_LANGUAGE_VERSION);
  if (infoLog)  InfoLog("  Shading Language Version : %s", str);
  else           printf("  Shading Language Version : %s\n", str);
  glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &value);
  if (infoLog)  InfoLog("  Maximum Vertex Array Size: %d vertices", value);
  else           printf("  Maximum Vertex Array Size: %d vertices\n", value);
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
  if (infoLog)  InfoLog("  Maximum Texture Size     : %d texels", value);
  else           printf("  Maximum Texture Size     : %d texels\n", value);
  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &value);
  if (infoLog)  InfoLog("  Maximum Vertex Attributes: %d", value);
  else           printf("  Maximum Vertex Attributes: %d\n", value);
  glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &value);
  if (infoLog)  InfoLog("  Maximum Vertex Uniforms  : %d", value);
  else           printf("  Maximum Vertex Uniforms  : %d\n", value);
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &value);
  if (infoLog)  InfoLog("  Maximum Texture Img Units: %d", value);
  else           printf("  Maximum Texture Img Units: %d\n", value);
  if (printExtensions)
  {
    str = glGetString(GL_EXTENSIONS);
    char *strLocal = (char *) malloc((strlen((char *) str)+1)*sizeof(char));
    if (NULL == strLocal)
    {
      if (infoLog)  InfoLog("  Supported Extensions     : %s", str);
      else           printf("  Supported Extensions     : %s\n", str);
    }
    else
    {
      strcpy(strLocal, (char *) str);
      if (infoLog)  InfoLog("  Supported Extensions     : %s", (strLocal = strtok(strLocal, " \t\n")));
      else           printf("  Supported Extensions     : %s\n", (strLocal = strtok(strLocal, " \t\n")));
      while ((strLocal = strtok(NULL, " \t\n")) != NULL)
      {
        if (infoLog)  InfoLog("                             %s", strLocal);
        else           printf("                             %s\n", strLocal);
      }
    }
  }
  if (infoLog)  InfoLog("");
  else      printf("\n");
}


/******************************************************************************
 Save States and NVRAM
 
 Save states and NVRAM use the same basic format. When anything changes that
 breaks compatibility with previous versions of Supermodel, the save state
 and NVRAM version numbers must be incremented as needed.
 
 Header block name: "Supermodel Save State" or "Supermodel NVRAM State"
 Data: Save state file version (4-byte integer), ROM set ID (up to 9 bytes, 
 including terminating \0).
 
 Different subsystems output their own blocks.
******************************************************************************/

static const int STATE_FILE_VERSION = 2;  // save state file version
static const int NVRAM_FILE_VERSION = 0;  // NVRAM file version
static unsigned s_saveSlot = 0;           // save state slot #

static void SaveState(IEmulator *Model3)
{
  CBlockFile  SaveState;
  
  std::string file_path = Util::Format() << "Saves/" << Model3->GetGame().name << ".st" << s_saveSlot;
  if (OKAY != SaveState.Create(file_path, "Supermodel Save State", "Supermodel Version " SUPERMODEL_VERSION))
  {
    ErrorLog("Unable to save state to '%s'.", file_path.c_str());
    return;
  }
  
  // Write file format version and ROM set ID to header block 
  int32_t fileVersion = STATE_FILE_VERSION;
  SaveState.Write(&fileVersion, sizeof(fileVersion));
  SaveState.Write(Model3->GetGame().name);
  
  // Save state
  Model3->SaveState(&SaveState);
  SaveState.Close();
  printf("Saved state to '%s'.\n", file_path.c_str());
  DebugLog("Saved state to '%s'.\n", file_path.c_str());
}

static void LoadState(IEmulator *Model3, std::string file_path = std::string())
{
  CBlockFile  SaveState;
  
  // Generate file path
  if (file_path.empty())
    file_path = Util::Format() << "Saves/" << Model3->GetGame().name << ".st" << s_saveSlot;
  
  // Open and check to make sure format is correct
  if (OKAY != SaveState.Load(file_path))
  {
    ErrorLog("Unable to load state from '%s'.", file_path.c_str());
    return;
  }
  
  if (OKAY != SaveState.FindBlock("Supermodel Save State"))
  {
    ErrorLog("'%s' does not appear to be a valid save state file.", file_path.c_str());
    return;
  }
  
  int32_t fileVersion;
  SaveState.Read(&fileVersion, sizeof(fileVersion));
  if (fileVersion != STATE_FILE_VERSION)
  {
    ErrorLog("'%s' is incompatible with this version of Supermodel.", file_path.c_str());
    return;
  }
  
  // Load
  Model3->LoadState(&SaveState);
  SaveState.Close();
  printf("Loaded state from '%s'.\n", file_path.c_str());
  DebugLog("Loaded state from '%s'.\n", file_path.c_str());
}

static void SaveNVRAM(IEmulator *Model3)
{
  CBlockFile  NVRAM;
  
  std::string file_path = Util::Format() << "NVRAM/" << Model3->GetGame().name << ".nv";
  if (OKAY != NVRAM.Create(file_path, "Supermodel NVRAM State", "Supermodel Version " SUPERMODEL_VERSION))
  {
    ErrorLog("Unable to save NVRAM to '%s'. Make sure directory exists!", file_path.c_str());
    return;
  }
  
  // Write file format version and ROM set ID to header block 
  int32_t fileVersion = NVRAM_FILE_VERSION;
  NVRAM.Write(&fileVersion, sizeof(fileVersion));
  NVRAM.Write(Model3->GetGame().name);
  
  // Save NVRAM
  Model3->SaveNVRAM(&NVRAM);
  NVRAM.Close();
  DebugLog("Saved NVRAM to '%s'.\n", file_path.c_str());
}

static void LoadNVRAM(IEmulator *Model3)
{
  CBlockFile  NVRAM;
  
  // Generate file path
  std::string file_path = Util::Format() << "NVRAM/" << Model3->GetGame().name << ".nv";
  
  // Open and check to make sure format is correct
  if (OKAY != NVRAM.Load(file_path))
  {
    //ErrorLog("Unable to restore NVRAM from '%s'.", filePath);
    return;
  }
  
  if (OKAY != NVRAM.FindBlock("Supermodel NVRAM State"))
  {
    ErrorLog("'%s' does not appear to be a valid NVRAM file.", file_path.c_str());
    return;
  }
  
  int32_t fileVersion;
  NVRAM.Read(&fileVersion, sizeof(fileVersion));
  if (fileVersion != NVRAM_FILE_VERSION)
  {
    ErrorLog("'%s' is incompatible with this version of Supermodel.", file_path.c_str());
    return;
  }
  
  // Load
  Model3->LoadNVRAM(&NVRAM);
  NVRAM.Close();
  DebugLog("Loaded NVRAM from '%s'.\n", file_path.c_str());
}


/******************************************************************************
 UI Rendering
 
 Currently, only does crosshairs for light gun games.
******************************************************************************/

static void GunToViewCoords(float *x, float *y)
{
  *x = (*x-150.0f)/(651.0f-150.0f); // Scale [150,651] -> [0.0,1.0]
  *y = (*y-80.0f)/(465.0f-80.0f);   // Scale [80,465] -> [0.0,1.0]
}

static void DrawCrosshair(float x, float y, float r, float g, float b)
{
  float base = 0.01f, height = 0.02f; // geometric parameters of each triangle
  float dist = 0.004f;          // distance of triangle tip from center
  float a = (float)xRes/(float)yRes;  // aspect ratio (to square the crosshair)
  
  glColor3f(r, g, b);
  glVertex2f(x, y+dist);  // bottom triangle
  glVertex2f(x+base/2.0f, y+(dist+height)*a);
  glVertex2f(x-base/2.0f, y+(dist+height)*a); 
  glVertex2f(x, y-dist);  // top triangle
  glVertex2f(x-base/2.0f, y-(dist+height)*a);
  glVertex2f(x+base/2.0f, y-(dist+height)*a);
  glVertex2f(x-dist, y);  // left triangle
  glVertex2f(x-dist-height, y+(base/2.0f)*a);
  glVertex2f(x-dist-height, y-(base/2.0f)*a);
  glVertex2f(x+dist, y);  // right triangle
  glVertex2f(x+dist+height, y-(base/2.0f)*a);
  glVertex2f(x+dist+height, y+(base/2.0f)*a);
}

/*
static void PrintGLError(GLenum error)
{
  switch (error)
  {
  case GL_INVALID_ENUM:       printf("invalid enum\n"); break;
  case GL_INVALID_VALUE:      printf("invalid value\n"); break;
  case GL_INVALID_OPERATION:  printf("invalid operation\n"); break;
  case GL_STACK_OVERFLOW:     printf("stack overflow\n"); break;
  case GL_STACK_UNDERFLOW:    printf("stack underflow\n"); break;
  case GL_OUT_OF_MEMORY:      printf("out of memory\n"); break;
  case GL_TABLE_TOO_LARGE:    printf("table too large\n"); break;
  case GL_NO_ERROR:           break;
  default:                    printf("unknown error\n"); break;
  }
}
*/

static void UpdateCrosshairs(CInputs *Inputs, unsigned crosshairs)
{
  float x[2], y[2];
  
  crosshairs &= 3;
  if (!crosshairs)
    return;
    
  // Set up the viewport and orthogonal projection
  glUseProgram(0);    // no shaders
  glViewport(xOffset, yOffset, xRes, yRes);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0.0, 1.0, 1.0, 0.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_TEXTURE_2D); // no texture mapping
  glDisable(GL_BLEND);    // no blending
  glDisable(GL_DEPTH_TEST); // no Z-buffering needed  
  glDisable(GL_LIGHTING);
  
  // Convert gun coordinates to viewspace coordinates
  x[0] = (float) Inputs->gunX[0]->value;
  y[0] = (float) Inputs->gunY[0]->value;
  x[1] = (float) Inputs->gunX[1]->value;
  y[1] = (float) Inputs->gunY[1]->value;
  GunToViewCoords(&x[0], &y[0]);
  GunToViewCoords(&x[1], &y[1]);
  
  // Draw visible crosshairs  
  glBegin(GL_TRIANGLES);
  if ((crosshairs & 1) && !Inputs->trigger[0]->offscreenValue)  // Player 1
    DrawCrosshair(x[0], y[0], 1.0f, 0.0f, 0.0f);
  if ((crosshairs & 2) && !Inputs->trigger[1]->offscreenValue)  // Player 2
    DrawCrosshair(x[1], y[1], 0.0f, 1.0f, 0.0f);
  glEnd();
  
  //PrintGLError(glGetError());
}


/******************************************************************************
 Video Callbacks
******************************************************************************/

static CInputs *videoInputs = NULL;

bool BeginFrameVideo()
{
  return true;
}

void EndFrameVideo()
{
  // Show crosshairs for light gun games
  if (videoInputs)
    UpdateCrosshairs(videoInputs, s_runtime_config["Crosshairs"].ValueAs<unsigned>());

  // Swap the buffers
  // TODO/FIXME - video_cb call here
}

/******************************************************************************
 Main Program Loop
******************************************************************************/

int Supermodel(const Game &game, ROMSet *rom_set, IEmulator *Model3, CInputs *Inputs, COutputs *Outputs)
{
  std::string initialState = s_runtime_config["InitStateFile"].ValueAs<std::string>();
  bool        gameHasLightguns = false;
  bool        quit = false;

  // Initialize and load ROMs
  if (OKAY != Model3->Init())
    return 1;
  if (Model3->LoadGame(game, *rom_set))
    return 1;
  *rom_set = ROMSet();  // free up this memory we won't need anymore
    
  // Load NVRAM
  LoadNVRAM(Model3);
    
  // Start up SDL and open a GL window
  char baseTitleStr[128];
  char titleStr[128];
  totalXRes = xRes = s_runtime_config["XResolution"].ValueAs<unsigned>();
  totalYRes = yRes = s_runtime_config["YResolution"].ValueAs<unsigned>();
  bool stretch = s_runtime_config["Stretch"].ValueAs<bool>();
  bool fullscreen = s_runtime_config["FullScreen"].ValueAs<bool>();
  if (OKAY != CreateGLScreen(baseTitleStr, &xOffset, &yOffset ,&xRes, &yRes, &totalXRes, &totalYRes, !stretch, fullscreen))
    return 1;

  // Info log GL information
  PrintGLInfo(false, true, false);
  
  // Initialize audio system
  if (OKAY != OpenAudio())
    return 1;

  // Hide mouse if fullscreen, enable crosshairs for gun games
  Inputs->GetInputSystem()->SetMouseVisibility(!s_runtime_config["FullScreen"].ValueAs<bool>());
  gameHasLightguns = !!(game.inputs & (Game::INPUT_GUN1|Game::INPUT_GUN2));
  if (gameHasLightguns)
    videoInputs = Inputs;
  else
    videoInputs = NULL;

  // Attach the inputs to the emulator
  Model3->AttachInputs(Inputs);

  // Attach the outputs to the emulator
  if (Outputs != NULL)
    Model3->AttachOutputs(Outputs);
  
  // Initialize the renderers
  CRender2D *Render2D = new CRender2D(s_runtime_config);
  IRender3D *Render3D = s_runtime_config["New3DEngine"].ValueAs<bool>() ? ((IRender3D *) new New3D::CNew3D(s_runtime_config, Model3->GetGame().name)) : ((IRender3D *) new Legacy3D::CLegacy3D(s_runtime_config));
  if (OKAY != Render2D->Init(xOffset, yOffset, xRes, yRes, totalXRes, totalYRes))
    goto QuitError;
  if (OKAY != Render3D->Init(xOffset, yOffset, xRes, yRes, totalXRes, totalYRes))
    goto QuitError;
  Model3->AttachRenderers(Render2D,Render3D);

  // Reset emulator
  Model3->Reset();
  
  // Load initial save state if requested
  if (initialState.length() > 0)
    LoadState(Model3, initialState);

  // Emulate!
  quit        = false;

  while (!quit)
  {
     Model3->RunFrame();
    
    // Poll the inputs
    if (!Inputs->Poll(&game, xOffset, yOffset, xRes, yRes))
      quit = true;
    

#if 0
    /* TODO/FIXME - retro_reset */
    else if (Inputs->uiReset->Pressed())
    {
       Model3->PauseThreads();
       SetAudioEnabled(false);

      // Reset emulator
      Model3->Reset();
      
      Model3->ResumeThreads();
      SetAudioEnabled(true);

      puts("Model 3 reset.");
    }
#endif
#if 0
    /* TODO/FIXME - Could be useful for context reset */
    else if (Inputs->uiFullScreen->Pressed())
    {
      // Toggle emulator fullscreen
      s_runtime_config.Get("FullScreen").SetValue(!s_runtime_config["FullScreen"].ValueAs<bool>());

      // Delete renderers and recreate them afterwards since GL context will most likely be lost when switching from/to fullscreen
      delete Render2D;
      delete Render3D;
      Render2D = NULL;
      Render3D = NULL;
      
      // Resize screen
      totalXRes = xRes = s_runtime_config["XResolution"].ValueAs<unsigned>();
      totalYRes = yRes = s_runtime_config["YResolution"].ValueAs<unsigned>();
      bool stretch = s_runtime_config["Stretch"].ValueAs<bool>();
      bool fullscreen = s_runtime_config["FullScreen"].ValueAs<bool>();
      if (OKAY != ResizeGLScreen(&xOffset,&yOffset,&xRes,&yRes,&totalXRes,&totalYRes,!stretch,fullscreen))
        goto QuitError;

      // Recreate renderers and attach to the emulator
      Render2D = new CRender2D(s_runtime_config);
    Render3D = s_runtime_config["New3DEngine"].ValueAs<bool>() ? ((IRender3D *) new New3D::CNew3D(s_runtime_config, Model3->GetGame().name)) : ((IRender3D *) new Legacy3D::CLegacy3D(s_runtime_config));
      if (OKAY != Render2D->Init(xOffset, yOffset, xRes, yRes, totalXRes, totalYRes))
        goto QuitError;
      if (OKAY != Render3D->Init(xOffset, yOffset, xRes, yRes, totalXRes, totalYRes))
        goto QuitError;
      Model3->AttachRenderers(Render2D,Render3D);
    
      Inputs->GetInputSystem()->SetMouseVisibility(!s_runtime_config["FullScreen"].ValueAs<bool>());
    }
    else if (Inputs->uiSaveState->Pressed())
    {
       Model3->PauseThreads();
       SetAudioEnabled(false);

      // Save game state
      SaveState(Model3);

      Model3->ResumeThreads();
      SetAudioEnabled(true);
    }
    else if (Inputs->uiChangeSlot->Pressed())
    {
      // Change save slot
      ++s_saveSlot;
      s_saveSlot %= 10; // clamp to [0,9]
      printf("Save slot: %d\n", s_saveSlot);
    }
    else if (Inputs->uiLoadState->Pressed())
    {
       Model3->PauseThreads();
       SetAudioEnabled(false);

      // Load game state
      LoadState(Model3);
            
      Model3->ResumeThreads();
      SetAudioEnabled(true);
    }
#endif
    if (Inputs->uiSelectCrosshairs->Pressed() && gameHasLightguns)
    {
      int crosshairs = (s_runtime_config["Crosshairs"].ValueAs<unsigned>() + 1) & 3;
      s_runtime_config.Get("Crosshairs").SetValue(crosshairs);
      switch (crosshairs)
      {
      case 0: puts("Crosshairs disabled.");             break;
      case 3: puts("Crosshairs enabled.");              break;
      case 1: puts("Showing Player 1 crosshair only."); break;
      case 2: puts("Showing Player 2 crosshair only."); break;
      }
    }
    else if (Inputs->uiClearNVRAM->Pressed())
    {
      // Clear NVRAM
      Model3->ClearNVRAM();
      puts("NVRAM cleared.");
    }
  }

  // Make sure all threads are paused before shutting down
  Model3->PauseThreads();
 
  // Save NVRAM
  SaveNVRAM(Model3);
  
  // Close audio
  CloseAudio();

  // Shut down renderers
  delete Render2D;
  delete Render3D;

  return 0;

  // Quit with an error
QuitError:
  delete Render2D;
  delete Render3D;
  return 1;
}


/******************************************************************************
 Entry Point and Command Line Procesing
******************************************************************************/

static const char s_configFilePath[] = { "Config/Supermodel.ini" };
static const char s_gameXMLFilePath[] = { "Config/Games.xml" };

// Create and configure inputs
static bool ConfigureInputs(CInputs *Inputs, Util::Config::Node *config, bool configure)
{
  static const char configFileComment[] = {
    ";\n"
    "; Supermodel Configuration File\n"
    ";\n"
  };
  
  Inputs->LoadFromConfig(*config);
    
  // If the user wants to configure the inputs, do that now
  if (configure)
  {
    // Open an SDL window 
    unsigned xOffset, yOffset, xRes=496, yRes=384;
    if (OKAY != CreateGLScreen("Supermodel - Configuring Inputs...", &xOffset, &yOffset, &xRes, &yRes, &totalXRes, &totalYRes, false, false))
      return (bool) ErrorLog("Unable to start SDL to configure inputs.\n");
    
    // Configure the inputs
    if (Inputs->ConfigureInputs(NULL, xOffset, yOffset, xRes, yRes))
    {
      // Write input configuration and input system settings to config file
      Inputs->StoreToConfig(config);
      Util::Config::WriteINIFile(s_configFilePath, *config, configFileComment);
    }
    else
      puts("Configuration aborted...");
    puts("");
  }
  
  return OKAY;
}

// Print game list
static void PrintGameList(const std::string &xml_file, const std::map<std::string, Game> &games)
{
  if (games.empty())
  {
    puts("No games defined.");
    return;
  }

  printf("Games defined in %s:\n", xml_file.c_str());
  puts("");
  puts("    ROM Set         Title");
  puts("    -------         -----");
  for (auto &v: games)
  {
    const Game &game = v.second;
    printf("    %s", game.name.c_str());
    for (int i = game.name.length(); i < 9; i++)  // pad for alignment (no game ID should be more than 9 letters)
      printf(" ");
    if (!game.version.empty())
      printf("       %s (%s)\n", game.title.c_str(), game.version.c_str());
    else
      printf("       %s\n", game.title.c_str());
  }
}

static void LogConfig(const Util::Config::Node &config)
{
  InfoLog("Runtime configuration:");
  for (auto &child: config)
  {
    if (child.Empty())
      InfoLog("  %s=<empty>", child.Key().c_str());
    else
      InfoLog("  %s=%s", child.Key().c_str(), child.ValueAs<std::string>().c_str());
  }
  InfoLog("");
}

static Util::Config::Node DefaultConfig()
{
  Util::Config::Node config("Global");
  config.Set("GameXMLFile", s_gameXMLFilePath);
  config.Set("InitStateFile", "");
  // CModel3
  config.Set("MultiThreaded", true);
  config.Set("GPUMultiThreaded", true);
  config.Set("PowerPCFrequency", "50");
  // 2D and 3D graphics engines
  config.Set("MultiTexture", false);
  config.Set("VertexShader", "");
  config.Set("FragmentShader", "");
  config.Set("VertexShaderFog", "");
  config.Set("FragmentShaderFog", "");
  config.Set("VertexShader2D", "");
  config.Set("FragmentShader2D", "");
  // CSoundBoard
  config.Set("EmulateSound", true);
  config.Set("Balance", false);
  // CDSB
  config.Set("EmulateDSB", true);
  config.Set("SoundVolume", "100");
  config.Set("MusicVolume", "100");
  // CDriveBoard
#ifdef SUPERMODEL_WIN32
  config.Set("ForceFeedback", false);
#endif
  // Platform-specific/UI 
  config.Set("New3DEngine", true);
  config.Set("XResolution", "496");
  config.Set("YResolution", "384");
  config.Set("FullScreen", false);
  config.Set("WideScreen", false);
  config.Set("Stretch", false);
  config.Set("VSync", true);
  config.Set("Throttle", true);
  config.Set("ShowFrameRate", false);
  config.Set("Crosshairs", int(0));
  config.Set("FlipStereo", false);
#ifdef SUPERMODEL_WIN32
  config.Set("InputSystem", "dinput");
  // DirectInput ForceFeedback
  config.Set("DirectInputConstForceLeftMax", "100");
  config.Set("DirectInputConstForceRightMax", "100");
  config.Set("DirectInputSelfCenterMax", "100");
  config.Set("DirectInputFrictionMax", "100");
  config.Set("DirectInputVibrateMax", "100");
  // XInput ForceFeedback
  config.Set("XInputConstForceThreshold", "30");
  config.Set("XInputConstForceMax", "100");
  config.Set("XInputVibrateMax", "100");
#ifdef NET_BOARD
  // NetBoard
  config.Set("EmulateNet", false);
#endif
#else
  config.Set("InputSystem", "sdl");
#endif
  config.Set("Outputs", "none");
  return config;
}

static void Title(void)
{
  puts("Supermodel: A Sega Model 3 Arcade Emulator (Version " SUPERMODEL_VERSION ")");
  puts("Copyright 2011-2018 by Bart Trzynadlowski, Nik Henson, Ian Curtis,");
  puts("                       Harry Tuttle, and Spindizzi\n");
}

static void Help(void)
{
  Util::Config::Node defaultConfig = DefaultConfig();
  puts("Usage: Supermodel <romset> [options]");
  puts("ROM set must be a valid ZIP file containing a single game.");
  puts("");
  puts("General Options:");
  puts("  -?, -h, -help, --help   Print this help text");
  puts("  -print-games            List supported games and quit");
  printf("  -game-xml-file=<file>   ROM set definition file [Default: %s]\n", s_gameXMLFilePath);
  puts("");
  puts("Core Options:");
  printf("  -ppc-frequency=<freq>   PowerPC frequency in MHz [Default: %d]\n", defaultConfig["PowerPCFrequency"].ValueAs<unsigned>());
  puts("  -no-threads             Disable multi-threading entirely");
  puts("  -gpu-multi-threaded     Run graphics rendering in separate thread [Default]");
  puts("  -no-gpu-thread          Run graphics rendering in main thread");
  puts("  -load-state=<file>      Load save state after starting");
  puts("");
  puts("Video Options:");
  puts("  -res=<x>,<y>            Resolution [Default: 496,384]");
  puts("  -window                 Windowed mode [Default]");
  puts("  -fullscreen             Full screen mode");
  puts("  -wide-screen            Expand 3D field of view to screen width");
  puts("  -stretch                Fit viewport to resolution, ignoring aspect ratio");
  puts("  -no-throttle            Disable 60 Hz frame rate lock");
  puts("  -vsync                  Lock to vertical refresh rate [Default]");
  puts("  -no-vsync               Do not lock to vertical refresh rate");
  puts("  -show-fps               Display frame rate in window title bar");
  puts("  -crosshairs=<n>         Crosshairs configuration for gun games:");
  puts("                           0=none [Default], 1=P1 only, 2=P2 only, 3=P1 & P2");
  puts("  -new3d                  New 3D engine by Ian Curtis [Default]");
  puts("  -legacy3d               Legacy 3D engine (faster but less accurate)");
  puts("  -multi-texture          Use 8 texture maps for decoding (legacy engine)");
  puts("  -no-multi-texture       Decode to single texture (legacy engine) [Default]");
  puts("  -vert-shader=<file>     Load Real3D vertex shader for 3D rendering");
  puts("  -frag-shader=<file>     Load Real3D fragment shader for 3D rendering");
  puts("  -vert-shader-fog=<file> Load Real3D scroll fog vertex shader (new engine)");
  puts("  -frag-shader-fog=<file> Load Real3D scroll fog fragment shader (new engine)");
  puts("  -vert-shader-2d=<file>  Load tile map vertex shader");
  puts("  -frag-shader-2d=<file>  Load tile map fragment shader");
  puts("  -print-gl-info          Print OpenGL driver information and quit");
  puts("");
  puts("Audio Options:");
  puts("  -sound-volume=<vol>     Volume of SCSP-generated sound in %, applies only");
  puts("                          when Digital Sound Board is present [Default: 100]");
  puts("  -music-volume=<vol>     Digital Sound Board volume in % [Default: 100]");
  puts("  -balance=<bal>          Relative front/rear balance in % [Default: 0]");
  puts("  -flip-stereo            Swap left and right audio channels");
  puts("  -no-sound               Disable sound board emulation (sound effects)");
  puts("  -no-dsb                 Disable Digital Sound Board (MPEG music)");
  puts("");
#ifdef NET_BOARD
  puts("Net Options:");
  puts("  -no-net                 Disable net board emulation (default)");
  puts("  -net                    Enable net board emulation (not working ATM - need -no-threads)");
  puts("");
#endif
  puts("Input Options:");
#ifdef SUPERMODEL_WIN32
  puts("  -force-feedback         Enable force feedback (DirectInput, XInput)");
#endif
  puts("  -config-inputs          Configure keyboards, mice, and game controllers");
#ifdef SUPERMODEL_WIN32
  printf("  -input-system=<s>       Input system [Default: %s]\n", defaultConfig["InputSystem"].ValueAs<std::string>().c_str());
  printf("  -outputs=<s>            Outputs [Default: %s]\n", defaultConfig["Outputs"].ValueAs<std::string>().c_str());
#endif
  puts("  -print-inputs           Prints current input configuration");
  puts("");
}

struct ParsedCommandLine
{
  Util::Config::Node config = Util::Config::Node("CommandLine");
  std::vector<std::string> rom_files;
  bool print_help = false;
  bool print_games = false;
  bool print_gl_info = false;
  bool config_inputs = false;
  bool print_inputs = false;
  bool disable_debugger = false;
  bool enter_debugger = false;
};

static ParsedCommandLine ParseCommandLine(int argc, char **argv)
{
  ParsedCommandLine cmd_line;
  const std::map<std::string, std::string> valued_options
  { // -option=value
    { "-game-xml-file",         "GameXMLFile"             },
    { "-load-state",            "InitStateFile"           },
    { "-ppc-frequency",         "PowerPCFrequency"        },
    { "-crosshairs",            "Crosshairs"              },
    { "-vert-shader",           "VertexShader"            },
    { "-frag-shader",           "FragmentShader"          },
    { "-vert-shader-fog",       "VertexShaderFog"         },
    { "-frag-shader-fog",       "FragmentShaderFog"       },
    { "-vert-shader-2d",        "VertexShader2D"          },
    { "-frag-shader-2d",        "FragmentShader2D"        },
    { "-sound-volume",          "SoundVolume"             },
    { "-music-volume",          "MusicVolume"             },
    { "-balance",               "Balance"                 },
    { "-input-system",          "InputSystem"             },
    { "-outputs",               "Outputs"                 }
  };
  const std::map<std::string, std::pair<std::string, bool>> bool_options
  { // -option
    { "-threads",             { "MultiThreaded",    true } },
    { "-no-threads",          { "MultiThreaded",    false } },
    { "-gpu-multi-threaded",  { "GPUMultiThreaded", true } },
    { "-no-gpu-thread",       { "GPUMultiThreaded", false } },
    { "-window",              { "FullScreen",       false } },
    { "-fullscreen",          { "FullScreen",       true } },
    { "-no-wide-screen",      { "WideScreen",       false } },
    { "-wide-screen",         { "WideScreen",       true } },
    { "-stretch",             { "Stretch",          true } },
    { "-no-stretch",          { "Stretch",          false } },
    { "-no-multi-texture",    { "MultiTexture",     false } },
    { "-multi-texture",       { "MultiTexture",     true } },
    { "-throttle",            { "Throttle",         true } },
    { "-no-throttle",         { "Throttle",         false } },
    { "-vsync",               { "VSync",            true } },
    { "-no-vsync",            { "VSync",            false } },
    { "-show-fps",            { "ShowFrameRate",    true } },
    { "-no-fps",              { "ShowFrameRate",    false } },
    { "-new3d",               { "New3DEngine",      true } },
    { "-legacy3d",            { "New3DEngine",      false } },
    { "-no-flip-stereo",      { "FlipStereo",       false } },
    { "-flip-stereo",         { "FlipStereo",       true } },
    { "-sound",               { "EmulateSound",     true } },
    { "-no-sound",            { "EmulateSound",     false } },
    { "-dsb",                 { "EmulateDSB",       true } },
    { "-no-dsb",              { "EmulateDSB",       false } },
#ifdef NET_BOARD
  { "-net",                   { "EmulateNet",       true } },
  { "-no-net",                { "EmulateNet",       false } },
#endif
#ifdef SUPERMODEL_WIN32
    { "-no-force-feedback",   { "ForceFeedback",    false } },
    { "-force-feedback",      { "ForceFeedback",    true } },
#endif
    
  };
  for (int i = 1; i < argc; i++)
  {
    std::string arg(argv[i]);
    if (arg[0] == '-')
    {
      // First, check maps
      size_t idx_equals = arg.find_first_of('=');
      if (idx_equals != std::string::npos)
      {
        std::string option(arg.begin(), arg.begin() + idx_equals);
        std::string value(arg.begin() + idx_equals + 1, arg.end());
        if (value.length() == 0)
        {
          ErrorLog("Argument to '%s' cannot be blank.", option.c_str());
          continue;
        }
        auto it = valued_options.find(option);
        if (it != valued_options.end())
        {
          const std::string &config_key = it->second;
          cmd_line.config.Set(config_key, value);
          continue;
        }
      }
      else
      {
        auto it = bool_options.find(arg);
        if (it != bool_options.end())
        {
          const std::string &config_key = it->second.first;
          bool value = it->second.second;
          cmd_line.config.Set(config_key, value);
          continue;
        }
        else if (valued_options.find(arg) != valued_options.end())
        {
          ErrorLog("'%s' requires an argument.", argv[i]);
          continue;
        }
      }
      // Fell through -- handle special cases
      if (arg == "-?" || arg == "-h" || arg == "-help" || arg == "--help")
        cmd_line.print_help = true;
      else if (arg == "-print-games")
        cmd_line.print_games = true;
      else if (arg == "-res" || arg.find("-res=") == 0)
      {
        std::vector<std::string> parts = Util::Format(arg).Split('=');
        if (parts.size() != 2)
          ErrorLog("'-res' requires both a width and height (e.g., '-res=496,384').");
        else
        {
          unsigned  x, y;
          if (2 == sscanf(&argv[i][4],"=%d,%d", &x, &y))
          {
            std::string xres = Util::Format() << x;
            std::string yres = Util::Format() << y;
            cmd_line.config.Set("XResolution", xres);
            cmd_line.config.Set("YResolution", yres);
          }
          else
            ErrorLog("'-res' requires both a width and height (e.g., '-res=496,384').");
        }
      }
      else if (arg == "-print-gl-info")
        cmd_line.print_gl_info = true;
      else if (arg == "-config-inputs")
        cmd_line.config_inputs = true;
      else if (arg == "-print-inputs")
        cmd_line.print_inputs = true;
      else
        ErrorLog("Ignoring unrecognized option: %s", argv[i]);
    }
    else
      cmd_line.rom_files.emplace_back(arg);
  }
  return cmd_line;
}

/*
 * main(argc, argv):
 *
 * Program entry point.
 */

int main(int argc, char **argv)
{

  Title();
  if (argc <= 1)
  {
    Help();
    return 0;
  }

  // Create default logger
  CFileLogger Logger(DEBUG_LOG_FILE, ERROR_LOG_FILE);
  Logger.ClearLogs();
  SetLogger(&Logger);
  InfoLog("Started as:");
  for (int i = 0; i < argc; i++)
    InfoLog("  argv[%d] = %s", i, argv[i]);
  InfoLog("");
  
  // Load config and parse command line
  auto cmd_line = ParseCommandLine(argc, argv);
  if (cmd_line.print_help)
  {
    Help();
    return 0;
  }
  if (cmd_line.print_gl_info)
  {
    PrintGLInfo(true, false, false);
    return 0;
  }
  bool print_games = cmd_line.print_games;
  bool rom_specified = !cmd_line.rom_files.empty();
  if (!rom_specified && !print_games && !cmd_line.config_inputs)
  {
    ErrorLog("No ROM file specified."); 
    return 0;
  }

  // Load game and resolve run-time config
  Game game;
  ROMSet rom_set;
  Util::Config::Node fileConfig("Global");
  Util::Config::Node fileConfigWithDefaults("Global");
  {
    Util::Config::Node config3("Global");
    Util::Config::Node config4("Global");
    Util::Config::FromINIFile(&fileConfig, s_configFilePath);
    Util::Config::MergeINISections(&fileConfigWithDefaults, DefaultConfig(), fileConfig); // apply .ini file's global section over defaults
    Util::Config::MergeINISections(&config3, fileConfigWithDefaults, cmd_line.config);    // apply command line overrides
    if (rom_specified || print_games)
    {
      std::string xml_file = config3["GameXMLFile"].ValueAs<std::string>();
      GameLoader loader(xml_file);
      if (print_games)
      {
        PrintGameList(xml_file, loader.GetGames());
        return 0;
      }
      if (loader.Load(&game, &rom_set, *cmd_line.rom_files.begin()))
        return 1;
      Util::Config::MergeINISections(&config4, config3, fileConfig[game.name]);   // apply game-specific config
    }
    else
      config4 = config3;
    Util::Config::MergeINISections(&s_runtime_config, config4, cmd_line.config);  // apply command line overrides once more
  }
  LogConfig(s_runtime_config);

  // Initialize SDL (individual subsystems get initialized later)
  if (SDL_Init(0) != 0)
  {
    ErrorLog("Unable to initialize SDL: %s\n", SDL_GetError());
    return 1;
  }
  
  // Create Model 3 emulator
  IEmulator *Model3 = new CModel3(s_runtime_config);
  
  // Create input system (default is SDL) and debugger
  CInputSystem *InputSystem = NULL;
  CInputs *Inputs = NULL;
  COutputs *Outputs = NULL;
  int exitCode = 0;

  // Create input system
  // NOTE: fileConfigWithDefaults is passed so that the global section is used
  // for input settings with default values populated
  std::string selectedInputSystem = s_runtime_config["InputSystem"].ValueAs<std::string>();
  if (selectedInputSystem == "sdl")
    InputSystem = new CSDLInputSystem();
  else
  {
    ErrorLog("Unknown input system: %s\n", selectedInputSystem.c_str());
    exitCode = 1;
    goto Exit;
  }

  // Create inputs from input system (configuring them if required)
  Inputs = new CInputs(InputSystem);
  if (!Inputs->Initialize())
  {
    ErrorLog("Unable to initalize inputs.\n");
    exitCode = 1;
    goto Exit;
  }
  
  // NOTE: fileConfig is passed so that the global section is used for input settings
  // and because this function may write out a new config file, which must preserve
  // all sections. We don't want to pollute the output with built-in defaults.
  if (ConfigureInputs(Inputs, &fileConfig, cmd_line.config_inputs))
  {
    exitCode = 1;
    goto Exit;
  }

  if (cmd_line.print_inputs)
  {
    Inputs->PrintInputs(NULL);
    InputSystem->PrintSettings();
  }
  
  if (!rom_specified)
    goto Exit;

  // Create outputs 
#ifdef SUPERMODEL_WIN32
  {
    std::string outputs = s_runtime_config["Outputs"].ValueAs<std::string>();
    if (outputs == "none")
      Outputs = NULL;
    else if (outputs == "win")
      Outputs = new CWinOutputs();
    else
    {
      ErrorLog("Unknown outputs: %s\n", outputs.c_str());
      exitCode = 1;
      goto Exit;
    }
  }
#endif // SUPERMODEL_WIN32

  // Initialize outputs
  if (Outputs != NULL && !Outputs->Initialize())
  {
    ErrorLog("Unable to initialize outputs.\n");
    exitCode = 1;
    goto Exit;
  }
  
  // Fire up Supermodel
  exitCode = Supermodel(game, &rom_set, Model3, Inputs, Outputs);
  delete Model3;

Exit:
  if (Inputs != NULL)
    delete Inputs;
  if (InputSystem != NULL)
    delete InputSystem;
  if (Outputs != NULL)
    delete Outputs;
  
  if (exitCode)
    InfoLog("Program terminated due to an error.");
  else
    InfoLog("Program terminated normally.");
    
  return exitCode;
}
