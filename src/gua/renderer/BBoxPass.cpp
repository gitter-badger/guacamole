/******************************************************************************
 * guacamole - delicious VR                                                   *
 *                                                                            *
 * Copyright: (c) 2011-2013 Bauhaus-Universität Weimar                        *
 * Contact:   felix.lauer@uni-weimar.de / simon.schneegans@uni-weimar.de      *
 *                                                                            *
 * This program is free software: you can redistribute it and/or modify it    *
 * under the terms of the GNU General Public License as published by the Free *
 * Software Foundation, either version 3 of the License, or (at your option)  *
 * any later version.                                                         *
 *                                                                            *
 * This program is distributed in the hope that it will be useful, but        *
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License   *
 * for more details.                                                          *
 *                                                                            *
 * You should have received a copy of the GNU General Public License along    *
 * with this program. If not, see <http://www.gnu.org/licenses/>.             *
 *                                                                            *
 ******************************************************************************/

// class header
#include <gua/renderer/BBoxPass.hpp>

#include <gua/renderer/GBuffer.hpp>
#include <gua/renderer/Pipeline.hpp>
#include <gua/databases/GeometryDatabase.hpp>
#include <gua/databases/Resources.hpp>
#include <gua/utils/Logger.hpp>

namespace gua {

////////////////////////////////////////////////////////////////////////////////

PipelinePassDescription* BBoxPassDescription::make_copy() const {
  return new BBoxPassDescription(*this);
}

////////////////////////////////////////////////////////////////////////////////

PipelinePass* BBoxPassDescription::make_pass(RenderContext const& ctx) const {
  auto pass = new PipelinePass{};

  pass->shader_ = std::make_shared<ShaderProgram>(),
  pass->shader_->create_from_sources(
    Resources::lookup_shader(Resources::shaders_bbox_vert),
    Resources::lookup_shader(Resources::shaders_bbox_geom),
    Resources::lookup_shader(Resources::shaders_bbox_frag)
  );

  pass->needs_color_buffer_as_input_ = false;
  pass->writes_only_color_buffer_ = true;

  pass->rasterizer_state_ = ctx.render_device->create_rasterizer_state(
    scm::gl::FILL_SOLID, scm::gl::CULL_NONE, scm::gl::ORIENT_CCW, false,
    false, 0.0f, false, true, scm::gl::point_raster_state(true)
  );
  pass->depth_stencil_state_ = nullptr;
  pass->blend_state_ = nullptr;

  auto count = 1;
  scm::gl::buffer_ptr buffer_ = ctx.render_device->create_buffer(
                                    scm::gl::BIND_VERTEX_BUFFER,
                                    scm::gl::USAGE_DYNAMIC_DRAW,
                                    count * 2 * sizeof(math::vec3),
                                    0);

  scm::gl::vertex_array_ptr vao_ = ctx.render_device->create_vertex_array(
    scm::gl::vertex_format(
        0, 0, scm::gl::TYPE_VEC3F, 2 * sizeof(math::vec3))(
        0, 1, scm::gl::TYPE_VEC3F, 2 * sizeof(math::vec3)), {buffer_});

  pass->process_ = [pass, buffer_, vao_](PipelinePassDescription* desc,Pipeline* pipe) {

    auto count(pipe->get_scene().bounding_boxes_.size());

    if (count > 0) {

      RenderContext const& ctx(pipe->get_context());

      ctx.render_device->resize_buffer(buffer_, count * 2 * sizeof(math::vec3));

      math::vec3* data(static_cast<math::vec3*>(ctx.render_context->map_buffer(
                              buffer_, scm::gl::ACCESS_WRITE_INVALIDATE_BUFFER)));

      for (int i(0); i<count; ++i) {
        data[2*i]   = pipe->get_scene().bounding_boxes_[i].min;
        data[2*i+1] = pipe->get_scene().bounding_boxes_[i].max;
      }


      ctx.render_context->unmap_buffer(buffer_);
      if (pass->rasterizer_state_)
        ctx.render_context->set_rasterizer_state(pass->rasterizer_state_);

      // bind gbuffer
      pipe->get_gbuffer().bind(ctx, pass);
      pipe->get_gbuffer().set_viewport(ctx);

      pass->shader_->use(ctx);
      pipe->bind_gbuffer_input(pass->shader_);
      ctx.render_context->bind_vertex_array(vao_);

      ctx.render_context->apply();
      ctx.render_context->draw_arrays(scm::gl::PRIMITIVE_POINT_LIST, 0, count);
      pipe->get_gbuffer().unbind(ctx);

      ctx.render_context->reset_state_objects();
    }
  };

  return pass;
}

////////////////////////////////////////////////////////////////////////////////

}