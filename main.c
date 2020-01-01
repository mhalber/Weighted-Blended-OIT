#define MSH_STD_INCLUDE_LIBC_HEADERS
#define MSH_STD_IMPLEMENTATION
#define MSH_ARGPARSE_IMPLEMENTATION
#define MSH_VEC_MATH_IMPLEMENTATION
#define MSH_CAMERA_IMPLEMENTATION
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "extern/glad.h"

#include "extern/msh_std.h"
#include "extern/msh_argparse.h"
#include "extern/msh_vec_math.h"
#include "extern/msh_camera.h"

#include "gl_utils.h"

/// INPUT
typedef struct mouse_state
{
  msh_vec2_t prev_pos;
  msh_vec2_t cur_pos;
  bool button[3];
  float scroll;
} mouse_state_t;

static mouse_state_t mouse = {0};
static bool oit_active = true;
static int oit_visualization_mode = 1;
static char* render_mode_strings[] = { "Weighted, Blended OIT", "Fixed order" };
static char* oit_visualization_strings[] = { "Final Framebuffer", "Accumulation Texture", "Revealage Texture" };

void scroll_callback( GLFWwindow* window, double xoffset, double yoffset )
{
  mouse.scroll = yoffset;
}

void cursor_pos_callback( GLFWwindow* window, double xpos, double ypos )
{
  mouse.prev_pos = mouse.cur_pos;
  mouse.cur_pos.x = xpos;
  mouse.cur_pos.y = ypos;
}

void mouse_button_callback( GLFWwindow* window, int button, int action, int mods)
{
  if( action == GLFW_PRESS )
  {
    if( button < 3 ) mouse.button[ button ] = 1;
  }
  if( action == GLFW_RELEASE )
  {
    if( button < 3 ) mouse.button[ button ] = 0;
  }
}

void key_callback( GLFWwindow* window, int key, int scancode, int action, int mods )
{
  if( action == GLFW_PRESS )
  {
    switch( key )
    {
      case GLFW_KEY_SPACE:
        oit_active = !oit_active;
        break;
      case GLFW_KEY_1:
        oit_visualization_mode = 1;
        break;
      case GLFW_KEY_2:
        oit_visualization_mode = 2;
        break;
      case GLFW_KEY_3:
        oit_visualization_mode = 3;
        break;
      default:
        break;
    }
  }
}

int32_t
main(int32_t argc, char **argv)
{
  int32_t error = 0;
  error = !glfwInit();
  if (error)
  {
    fprintf(stderr, "[] Failed to initialize glfw!\n");
    return EXIT_FAILURE;
  }

  int32_t window_width = 1024, window_height = 720;
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
  glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE );

  GLFWwindow *window = glfwCreateWindow(window_width, window_height, "Weighted, Blended OIT", NULL, NULL);
  if (!window)
  {
    fprintf(stderr, "[] Failed to create window!\n");
    return EXIT_FAILURE;
  }

  glfwSetKeyCallback( window, key_callback );
  glfwSetScrollCallback( window, scroll_callback );
  glfwSetMouseButtonCallback( window, mouse_button_callback );
  glfwSetCursorPosCallback( window, cursor_pos_callback );
  glfwMakeContextCurrent(window);
  
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
  {
    return EXIT_FAILURE;
  }
  
  GLint flags;
  glGetIntegerv( GL_CONTEXT_FLAGS, &flags );
  if( flags & GL_CONTEXT_FLAG_DEBUG_BIT )
  {
    glEnable( GL_DEBUG_OUTPUT );
    glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
    glDebugMessageCallback( gl_utils_debug_msg_call_back, NULL );
    glDebugMessageControl( GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_FALSE );
    glDebugMessageControl( GL_DONT_CARE, GL_DEBUG_TYPE_ERROR, GL_DONT_CARE, 0, NULL, GL_TRUE ); 
  }

/////// Full screen present shader
  const char* fullscreen_vs_src =
    GL_UTILS_SHDR_VERSION
    GL_UTILS_SHDR_SOURCE
    (
      layout( location = 0 ) in vec3 pos;

      out vec2 v_uv;
      void main()
      {
        v_uv = 0.5 * (pos.xy + 1.0);
        gl_Position = vec4( pos, 1.0 );
      }
    );

  const char* fullscreen_fs_src = 
    GL_UTILS_SHDR_VERSION
    GL_UTILS_SHDR_SOURCE
    (
      layout(location = 0) uniform sampler2D accum_tex;
      layout(location = 1) uniform sampler2D reveal_tex;
      layout(location = 2) uniform int oit_visualization_mode;
      out vec4 frag_col;
      in vec2 v_uv;
      void main() 
      {
        float revealage = texture( reveal_tex, v_uv ).r;
        vec4 accum = texture( accum_tex, v_uv );  
        if( oit_visualization_mode == 2 )
        {
          frag_col = vec4( accum.rgb, 0.0 );
        }
        else if( oit_visualization_mode == 3 )
        {
          frag_col = vec4( 1.0 - revealage );
        }
        else
        {
          if (revealage == 1.0) { discard; }
          vec3 avg_col = accum.rgb / clamp( accum.a, 1e-3, 5e4 );
          frag_col = vec4( avg_col, revealage );
        }
      }
    );
  
  GLuint fullscreen_vertex_shader = glCreateShader( GL_VERTEX_SHADER );
  GLuint fullscreen_fragment_shader = glCreateShader( GL_FRAGMENT_SHADER );

  glShaderSource( fullscreen_vertex_shader, 1, &fullscreen_vs_src, 0 );
  glCompileShader( fullscreen_vertex_shader );
  gl_utils_assert_shader_compiled( fullscreen_vertex_shader, "FULLSCREEN VERTEX SHADER" );

  glShaderSource( fullscreen_fragment_shader, 1, &fullscreen_fs_src, 0 );
  glCompileShader( fullscreen_fragment_shader );
  gl_utils_assert_shader_compiled( fullscreen_fragment_shader, "FULLSCREEN FRAGMENT SHADER" );
  
  GLuint fullscreen_program_id = glCreateProgram();
  glAttachShader( fullscreen_program_id, fullscreen_vertex_shader );
  glAttachShader( fullscreen_program_id, fullscreen_fragment_shader );
  
  glLinkProgram( fullscreen_program_id );
  
  glDetachShader( fullscreen_program_id, fullscreen_vertex_shader );
  glDetachShader( fullscreen_program_id, fullscreen_fragment_shader );

  glDeleteShader( fullscreen_vertex_shader );
  glDeleteShader( fullscreen_fragment_shader );


/////// Transparency Shader
  const char* transparency_vs_src = 
    GL_UTILS_SHDR_VERSION
    GL_UTILS_SHDR_SOURCE
    (
      layout( location = 0 ) in vec3 pos;
      layout( location = 1 ) in vec4 col;
      layout( location = 2 ) in vec2 uv;

      layout( location = 0 ) uniform mat4 mvp;
      layout( location = 1 ) uniform mat4 view;
      out vec4 v_col;
      out vec2 v_uv;
      void main()
      {
        v_col = col;
        v_uv = uv;
        gl_Position = mvp * vec4( pos, 1.0 );
      }
    );

  const char* transparency_fs_src = 
    GL_UTILS_SHDR_VERSION
    GL_UTILS_SHDR_SOURCE
    (
      in vec4 v_col; \n
      in vec2 v_uv; \n
      layout( location = 0 ) out vec4 accum; \n
      layout( location = 1 ) out float revelage; \n
      void main() \n
      {
        float dist = sqrt( v_uv.x * v_uv.x + v_uv.y * v_uv.y );
        vec4 out_col = v_col;
        out_col.a *= (1.0 - smoothstep(0.85, 1.0, dist) );

        float csz = abs(1.0 / gl_FragCoord.w);

#if 1
        // Equation 9
        float factor = 1.0/200.0; 
        float z = factor * csz;
        float weight = clamp( (0.03 / (1e-5 + pow(z, 4.0) ) ), 1e-4, 3e3 );
#else
        // Exponential function with coefficents fit to better resemple fixed order.
        // Sensitve to changes in depth
        float a = 58.3765228;
        float b = 1.45434782;
        float c = 0.00630901288;
        float fz = a * exp(-b*csz) + c;
        float weight = clamp(fz, 1e-2, 3e3);
#endif
        accum = vec4( out_col.rgb * out_col.a, out_col.a ) * weight;
        revelage = out_col.a;
      } 
    );
  
  GLuint transparency_vertex_shader = glCreateShader( GL_VERTEX_SHADER );
  GLuint transparency_fragment_shader = glCreateShader( GL_FRAGMENT_SHADER );

  glShaderSource( transparency_vertex_shader, 1, &transparency_vs_src, 0 );
  glCompileShader( transparency_vertex_shader );
  gl_utils_assert_shader_compiled( transparency_vertex_shader, "TRANSPARENCY VERTEX SHADER" );

  glShaderSource( transparency_fragment_shader, 1, &transparency_fs_src, 0 );
  glCompileShader( transparency_fragment_shader );
  gl_utils_assert_shader_compiled( transparency_fragment_shader, "TRANSPARENCY FRAGMENT SHADER" );
  
  GLuint transparency_program_id = glCreateProgram();
  glAttachShader( transparency_program_id, transparency_vertex_shader );
  glAttachShader( transparency_program_id, transparency_fragment_shader );
  
  glLinkProgram( transparency_program_id );
  
  glDetachShader( transparency_program_id, transparency_vertex_shader );
  glDetachShader( transparency_program_id, transparency_fragment_shader );

  glDeleteShader( transparency_vertex_shader );
  glDeleteShader( transparency_fragment_shader );


/////// Standard Shader (requires sorting)
  const char* sorted_vs_src = 
    GL_UTILS_SHDR_VERSION
    GL_UTILS_SHDR_SOURCE
    (
      layout( location = 0 ) in vec3 pos;
      layout( location = 1 ) in vec4 col;
      layout( location = 2 ) in vec2 uv;

      layout( location = 0 ) uniform mat4 mvp;
      out vec4 v_col;
      out vec2 v_uv;
      void main()
      {
        v_col = col;
        v_uv = uv;
        gl_Position = mvp * vec4( pos, 1.0 );
      }
    );

  const char* sorted_fs_src = 
    GL_UTILS_SHDR_VERSION
    GL_UTILS_SHDR_SOURCE
    (
      in vec4 v_col; \n
      in vec2 v_uv; \n
      out vec4 frag_col; \n
      void main() \n
      {
        float dist = sqrt( v_uv.x * v_uv.x + v_uv.y * v_uv.y );
        frag_col = v_col;
        frag_col.a *= (1.0 - smoothstep(0.85, 1.0, dist) );
      }
    );
  
  GLuint sorted_vertex_shader = glCreateShader( GL_VERTEX_SHADER );
  GLuint sorted_fragment_shader = glCreateShader( GL_FRAGMENT_SHADER );

  glShaderSource( sorted_vertex_shader, 1, &sorted_vs_src, 0 );
  glCompileShader( sorted_vertex_shader );
  gl_utils_assert_shader_compiled( sorted_vertex_shader, "SORTED VERTEX SHADER" );

  glShaderSource( sorted_fragment_shader, 1, &sorted_fs_src, 0 );
  glCompileShader( sorted_fragment_shader );
  gl_utils_assert_shader_compiled( sorted_fragment_shader, "SORTED FRAGMENT SHADER" );
  
  GLuint sorted_program_id = glCreateProgram();
  glAttachShader( sorted_program_id, sorted_vertex_shader );
  glAttachShader( sorted_program_id, sorted_fragment_shader );
  
  glLinkProgram( sorted_program_id );
  
  glDetachShader( sorted_program_id, sorted_vertex_shader );
  glDetachShader( sorted_program_id, sorted_fragment_shader );

  glDeleteShader( sorted_vertex_shader );
  glDeleteShader( sorted_fragment_shader );

////////

 int32_t binding_idx = 0;

/////// Fullscreen 'quad' geometry setup
  float fullscreen_quad_verts[] = { -1.0, -1.0, 0.0,
                                    -1.0,  3.0, 0.0,
                                     3.0, -1.0, 0.0 };

  GLuint fullscreen_quad_vao, fullscreen_quad_vbo;
  glCreateVertexArrays( 1, &fullscreen_quad_vao );
  glCreateBuffers( 1, &fullscreen_quad_vbo );

  glNamedBufferData( fullscreen_quad_vbo, sizeof(fullscreen_quad_verts), fullscreen_quad_verts, GL_STATIC_DRAW );

  glVertexArrayVertexBuffer( fullscreen_quad_vao, binding_idx, fullscreen_quad_vbo, 0, 3 * sizeof( float ) );
  glEnableVertexArrayAttrib( fullscreen_quad_vao, 0 );

  glVertexArrayAttribFormat( fullscreen_quad_vao, 0, 3, GL_FLOAT, GL_FALSE, 0 );

  glVertexArrayAttribBinding( fullscreen_quad_vao, 0, binding_idx );


/////// Transparent quads geometry setup 
  float quads_verts[] = 
  {
    -1.0, -1.0,  0.0,   0.0, 0.0, 1.0, 0.75,   -1.0, -1.0,
    -1.0,  1.0,  0.0,   0.0, 0.0, 1.0, 0.75,   -1.0,  1.0,
     1.0,  1.0,  0.0,   0.0, 0.0, 1.0, 0.75,    1.0,  1.0,
     1.0, -1.0,  0.0,   0.0, 0.0, 1.0, 0.75,    1.0, -1.0,

    -1.0, -1.0, -2.0,   1.0, 0.0, 0.0, 0.75,   -1.0, -1.0,
    -1.0,  1.0, -2.0,   1.0, 0.0, 0.0, 0.75,   -1.0,  1.0,
     1.0,  1.0, -2.0,   1.0, 0.0, 0.0, 0.75,    1.0,  1.0,
     1.0, -1.0, -2.0,   1.0, 0.0, 0.0, 0.75,    1.0, -1.0,

    -1.0, -1.0,  -1.0,   1.0, 1.0, 0.0, 0.75,   -1.0, -1.0,
    -1.0,  1.0,  -1.0,   1.0, 1.0, 0.0, 0.75,   -1.0,  1.0,
     1.0,  1.0,  -1.0,   1.0, 1.0, 0.0, 0.75,    1.0,  1.0,
     1.0, -1.0,  -1.0,   1.0, 1.0, 0.0, 0.75,    1.0, -1.0,
  };

  uint16_t quads_ind[] = { 0, 1, 2,  0, 2, 3,
                           4, 5, 6,  4, 6, 7,
                           8, 9, 10, 8, 10, 11 };
  GLuint quads_vao, quads_oit_vao, quads_vbo, quads_ebo;

  glCreateVertexArrays( 1, &quads_oit_vao );
  glCreateVertexArrays( 1, &quads_vao );
  glCreateBuffers( 1, &quads_vbo );
  glCreateBuffers( 1, &quads_ebo );

  glNamedBufferData( quads_vbo, sizeof(quads_verts), quads_verts, GL_STATIC_DRAW );
  glNamedBufferData( quads_ebo, sizeof(quads_ind), quads_ind, GL_STATIC_DRAW );

  glVertexArrayVertexBuffer( quads_oit_vao, binding_idx, quads_vbo, 0, 9 * sizeof( float ) );
  glVertexArrayElementBuffer( quads_oit_vao, quads_ebo );

  glEnableVertexArrayAttrib( quads_oit_vao, 0 );
  glEnableVertexArrayAttrib( quads_oit_vao, 1 );
  glEnableVertexArrayAttrib( quads_oit_vao, 2 );

  glVertexArrayAttribFormat( quads_oit_vao, 0, 3, GL_FLOAT, GL_FALSE, 0 );
  glVertexArrayAttribFormat( quads_oit_vao, 1, 4, GL_FLOAT, GL_FALSE, 3 * sizeof(float) );
  glVertexArrayAttribFormat( quads_oit_vao, 2, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float) );

  glVertexArrayAttribBinding( quads_oit_vao, 0, binding_idx );
  glVertexArrayAttribBinding( quads_oit_vao, 1, binding_idx );
  glVertexArrayAttribBinding( quads_oit_vao, 2, binding_idx );

  glVertexArrayVertexBuffer( quads_vao, binding_idx, quads_vbo, 0, 9 * sizeof( float ) );

  glEnableVertexArrayAttrib( quads_vao, 0 );
  glEnableVertexArrayAttrib( quads_vao, 1 );
  glEnableVertexArrayAttrib( quads_vao, 2 );

  glVertexArrayAttribFormat( quads_vao, 0, 3, GL_FLOAT, GL_FALSE, 0 );
  glVertexArrayAttribFormat( quads_vao, 1, 4, GL_FLOAT, GL_FALSE, 3 * sizeof(float) );
  glVertexArrayAttribFormat( quads_vao, 2, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float) );

  glVertexArrayAttribBinding( quads_vao, 0, binding_idx );
  glVertexArrayAttribBinding( quads_vao, 1, binding_idx );
  glVertexArrayAttribBinding( quads_vao, 2, binding_idx );

//////// Framebuffer setup
  uint32_t oit_fbo_width = 1024;
  uint32_t oit_fbo_height = 720;
  GLuint oit_fbo, accum_texture, reveal_texture, rbo_depth;
  glGenFramebuffers( 1, &oit_fbo );
  glBindFramebuffer(GL_FRAMEBUFFER, oit_fbo );

  glGenTextures(1, &accum_texture);
  glBindTexture(GL_TEXTURE_2D, accum_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, oit_fbo_width, oit_fbo_height, 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, accum_texture, 0);

  glGenTextures(1, &reveal_texture);
  glBindTexture(GL_TEXTURE_2D, reveal_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, oit_fbo_width, oit_fbo_height, 0, GL_RED, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, reveal_texture, 0);

  GLuint attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
  glDrawBuffers(2, attachments);

  glGenRenderbuffers( 1, &rbo_depth );
  glBindRenderbuffer( GL_RENDERBUFFER, rbo_depth );
  glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT, oit_fbo_width, oit_fbo_height );
  glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_depth );

  if( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
  {
    printf( "Framebuffer not complete!\n");
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

//////// Camera setup
  msh_camera_t cam = {0};
  msh_camera_init(&cam, &(msh_camera_desc_t){.eye = msh_vec3( -2.0f, -1.2f, 3.0f ),
                                             .center = msh_vec3_zeros(),
                                             .up = msh_vec3_posy(),
                                             .viewport = msh_vec4(0.0f, 0.0f, window_width, window_height),
                                             .fovy = msh_rad2deg(60),
                                             .znear = 0.01f,
                                             .zfar = 500.0f,
                                             .use_ortho = false });
  msh_mat4_t mvp = msh_mat4_mul( cam.proj, cam.view );

  glEnable( GL_DEPTH_TEST );
  glEnable( GL_BLEND );
  glEnable( GL_FRAMEBUFFER_SRGB );
  while (!glfwWindowShouldClose(window))
  {
    // Update the camera
    glfwGetWindowSize(window, &window_width, &window_height);
    if (window_width != cam.viewport.z || window_height != cam.viewport.w)
    {
      cam.viewport.z = window_width;
      cam.viewport.w = window_height;
      msh_camera_update_proj(&cam);
      int32_t fb_w, fb_h;
      glfwGetFramebufferSize( window, &fb_w, &fb_h );
    }

    if( mouse.button[0] == 1 )
    {
      msh_camera_rotate( &cam, mouse.prev_pos, mouse.cur_pos );
      msh_camera_update_view( &cam );
    }

    if( mouse.button[1] == 1 )
    {
      msh_camera_pan( &cam, mouse.prev_pos, mouse.cur_pos );
      msh_camera_update_view( &cam );
    }

    if( mouse.scroll != 0 )
    {
      msh_camera_zoom( &cam, mouse.scroll );
      msh_camera_update_view( &cam );
      mouse.scroll = 0.0f;
    }

    mvp = msh_mat4_mul(cam.proj, cam.view);

    if( oit_active )
    {
      // Opaque pass goes here, nothing in this implementation, other than a clear
      glBindFramebuffer( GL_FRAMEBUFFER, 0 );
      glDepthMask( GL_TRUE );
      glClearColor( 1.0f, 0.75f, 0.75f, 1.0f );
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
      
      // Copy depth buffer
      glBindFramebuffer(GL_READ_FRAMEBUFFER, 0 );
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oit_fbo );
      glBlitFramebuffer( 0, 0, window_width, window_height, 0, 0, 1024, 720, GL_DEPTH_BUFFER_BIT, GL_NEAREST );

      // OIT pass
      glBindFramebuffer( GL_FRAMEBUFFER, oit_fbo );
      glViewport( 0, 0, 1024, 720 );
      glDepthMask( GL_FALSE );

      static const float clear_accum[] = { 0, 0, 0, 0 };
      glClearBufferfv( GL_COLOR, 0, clear_accum );
      glBlendEquationi( 0, GL_FUNC_ADD );
      glBlendFunci( 0, GL_ONE, GL_ONE );

      static const float clear_reveal[] = { 1, 1, 1, 1 };
      glClearBufferfv( GL_COLOR, 1, clear_reveal );
      glBlendEquationi( 1, GL_FUNC_ADD );
      glBlendFunci( 1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR );


      glUseProgram( transparency_program_id );
      glUniformMatrix4fv( 0, 1, GL_FALSE, &mvp.data[0] );
      glUniformMatrix4fv( 1, 1, GL_FALSE, &cam.view.data[0] );

      glBindVertexArray( quads_oit_vao );
      glDrawElements( GL_TRIANGLES, 18, GL_UNSIGNED_SHORT, NULL );

      // Composite over the default buffer.
      glBindFramebuffer( GL_FRAMEBUFFER, 0 );
      glClearColor( 0.75f, 0.75f, 0.75f, 1.0f );
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
      glDepthMask( GL_TRUE );
      glBlendEquation( GL_FUNC_ADD );
      glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
      glBlendFunc( GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA );
      glViewport(0, 0, window_width, window_height);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, accum_texture);

      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, reveal_texture);

      glUseProgram( fullscreen_program_id );
      glUniform1i( 0, 0 ); // Accum  texture
      glUniform1i( 1, 1 ); // Reveal texture
      glUniform1i( 2, oit_visualization_mode ); // Render mode
      glBindVertexArray( fullscreen_quad_vao );
      glDrawArrays( GL_TRIANGLES, 0, 3 );
    }
    else
    {
      // Draw sorted
      glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
      glBlendEquation( GL_FUNC_ADD );

      glClearColor( 0.75f, 0.75f, 0.75f, 1.0f );
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

      glUseProgram( sorted_program_id );
      glUniformMatrix4fv( 0, 1, GL_FALSE, &mvp.data[0] );

      glBindVertexArray( quads_vao );
      glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, quads_ind + 6);
      glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, quads_ind + 12);
      glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, quads_ind );
    }

    char buf[1024];
    if( !oit_active )
    {
      snprintf(buf, 1024, "%s\n", render_mode_strings[1] );
    }
    else
    {
    snprintf(buf, 1024, "%s: %s\n", render_mode_strings[0], oit_visualization_strings[oit_visualization_mode-1] );
    }
    glfwSetWindowTitle( window, buf );
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();
  return EXIT_SUCCESS;
}