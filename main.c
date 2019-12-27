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


void key_callback( GLFWwindow* window, int key, int scancode, int action, int mods )
{
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
    // glDebugMessageControl( GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_FALSE );
    // Turn errors on.
    // glDebugMessageControl( GL_DONT_CARE, GL_DEBUG_TYPE_ERROR, GL_DONT_CARE, 0, NULL, GL_TRUE ); 
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
      layout(location = 0) uniform sampler2D accum_tex; \n
      layout(location = 1) uniform sampler2D reveal_tex;  \n
      out vec4 frag_col;  \n
      in vec2 v_uv;  \n
      void main() \n
      { \n
        // vec3 col = v_uv.y * vec3(1.0, 0.0, 0.0) + (1.0 - v_uv.y) * vec3(0.0, 0.0, 0.8); \n
        // frag_col = vec4( 0.5 * texture( accum_tex, v_uv ).rgb + 0.5 * col, 1.0) ; \n
        frag_col = vec4( texture( reveal_tex, v_uv ).rrr, 1.0 );
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

      layout( location = 0 ) uniform mat4 mvp;
      out vec4 v_col;
      void main()
      {
        v_col = col;
        gl_Position = mvp * vec4( pos, 1.0 );
      }
    );

  const char* transparency_fs_src = 
    GL_UTILS_SHDR_VERSION
    GL_UTILS_SHDR_SOURCE
    (
      in vec4 v_col; \n
      layout( location = 0 ) out vec4 accum; \n
      layout( location = 1 ) out float revelage; \n
      void main() \n
      { \n
        float a = v_col.a; \n
        float w = clamp(pow(min(1.0, a * 10.0) + 0.01, 3.0) * 1e8 * pow(1.0 - gl_FragCoord.z * 0.9, 3.0), 1e-2, 3e3); \n
        accum = vec4( v_col.rgb * v_col.a, v_col.a );// * w; \n
        revelage = v_col.a;
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
    -1.0, -1.0,  0.0,   0.2, 0.4, 1.0, 0.5,
    -1.0,  1.0,  0.0,   0.2, 0.4, 1.0, 0.5,
     1.0,  1.0,  0.0,   0.2, 0.4, 1.0, 0.5,
     1.0, -1.0,  0.0,   0.2, 0.4, 1.0, 0.5,

    -1.2, -1.2,  0.2,   1.0, 0.2, 0.3, 0.5,
    -1.2,  0.8,  0.2,   1.0, 0.2, 0.3, 0.5,
     0.8,  0.8,  0.2,   1.0, 0.2, 0.3, 0.5,
     0.8, -1.2,  0.2,   1.0, 0.2, 0.3, 0.5,
    
    -0.8, -0.8, -0.2,   0.4, 1.0, 0.1, 0.5,
    -0.8,  1.2, -0.2,   0.4, 1.0, 0.1, 0.5,
     1.2,  1.2, -0.2,   0.4, 1.0, 0.1, 0.5,
     1.2, -0.8, -0.2,   0.4, 1.0, 0.1, 0.5,
  };


  uint16_t quads_ind[] = { 0, 1, 2,  0, 2, 3,
                          4, 5, 6,  4, 6, 7,
                          8, 9, 10, 8, 10, 11 };
  GLuint quads_vao, quads_vbo, quads_ebo;

  glCreateVertexArrays( 1, &quads_vao );
  glCreateBuffers( 1, &quads_vbo );
  glCreateBuffers( 1, &quads_ebo );

  glNamedBufferData( quads_vbo, sizeof(quads_verts), quads_verts, GL_STATIC_DRAW );
  glNamedBufferData( quads_ebo, sizeof(quads_ind), quads_ind, GL_STATIC_DRAW );

  glVertexArrayVertexBuffer( quads_vao, binding_idx, quads_vbo, 0, 7 * sizeof( float ) );
  glVertexArrayElementBuffer( quads_vao, quads_ebo );

  glEnableVertexArrayAttrib( quads_vao, 0 );
  glEnableVertexArrayAttrib( quads_vao, 1 );

  glVertexArrayAttribFormat( quads_vao, 0, 3, GL_FLOAT, GL_FALSE, 0 );
  glVertexArrayAttribFormat( quads_vao, 1, 4, GL_FLOAT, GL_FALSE, 3 * sizeof(float) );

  glVertexArrayAttribBinding( quads_vao, 0, binding_idx );
  glVertexArrayAttribBinding( quads_vao, 1, binding_idx );

//////// Framebuffer setup
  uint32_t oit_fbo_width = 1024;
  uint32_t oit_fbo_height = 720;
  GLuint oit_fbo, accum_texture, reveal_texture, rbo_depth;
  glGenFramebuffers( 1, &oit_fbo );
  glBindFramebuffer(GL_FRAMEBUFFER, oit_fbo );

  glGenTextures(1, &accum_texture);
  glBindTexture(GL_TEXTURE_2D, accum_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, oit_fbo_width, oit_fbo_height, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, accum_texture, 0);

  glGenTextures(1, &reveal_texture);
  glBindTexture(GL_TEXTURE_2D, reveal_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, oit_fbo_width, oit_fbo_height, 0, GL_RED, GL_HALF_FLOAT, NULL);
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
  msh_camera_init(&cam, &(msh_camera_desc_t){.eye = msh_vec3( 3.0f, 3.0f, 6.0f ),
                                             .center = msh_vec3_zeros(),
                                             .up = msh_vec3_posy(),
                                             .viewport = msh_vec4(0.0f, 0.0f, window_width, window_height),
                                             .fovy = msh_rad2deg(60),
                                             .znear = 0.01f,
                                             .zfar = 100.0f,
                                             .use_ortho = true });
  msh_mat4_t mvp = msh_mat4_mul(cam.proj, cam.view);


  glDisable( GL_BLEND );
  // glBlendEquation(GL_FUNC_ADD);
  // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glDisable( GL_DEPTH_TEST );
  while (!glfwWindowShouldClose(window))
  {
    // Update the camera
    glfwGetWindowSize(window, &window_width, &window_height);
    if (window_width != cam.viewport.z || window_height != cam.viewport.w)
    {
      cam.viewport.z = window_width;
      cam.viewport.w = window_height;
      msh_camera_update_proj(&cam);
      mvp = msh_mat4_mul(cam.proj, cam.view);
      int32_t fb_w, fb_h;
      glfwGetFramebufferSize( window, &fb_w, &fb_h );
    }
    
    glBindFramebuffer( GL_FRAMEBUFFER, oit_fbo );
    glViewport( 0, 0, 1024, 720 );

    static const float clear_accum[] = { 0, 0, 0, 0 };
    glClearBufferfv(GL_COLOR, 0, clear_accum);
    static const float clear_reveal[] = { 0, 0, 0, 0 };
    glClearBufferfv(GL_COLOR, 1, clear_reveal);

    glUseProgram( transparency_program_id );
    glUniformMatrix4fv( 0, 1, GL_FALSE, &mvp.data[0] );

    glBindVertexArray( quads_vao );
    glDrawElements( GL_TRIANGLES, 18, GL_UNSIGNED_SHORT, NULL );

    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, window_width, window_height);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, accum_texture);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, reveal_texture);

    glUseProgram( fullscreen_program_id );
    glUniform1i( 0, 0 ); // Accum  texture
    glUniform1i( 1, 1 ); // Reveal texture
    glBindVertexArray( fullscreen_quad_vao );
    glDrawArrays( GL_TRIANGLES, 0, 3 );

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();
  return EXIT_SUCCESS;
}