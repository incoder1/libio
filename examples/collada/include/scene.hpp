#ifndef __SCENE_HPP_INCLUDED__
#define __SCENE_HPP_INCLUDED__

#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace engine  {

class scene {
public:
	scene(float width, float height,float eye_distance,float depth);

	void rotate_model(float x_rad, float y_rad, float z_rad);

	void move_model_far(float distance);

	void move_model_near(float distance);


	const float* get_veiw_mat() noexcept;
	const float* get_mvp() noexcept;

private:
	glm::mat4 model_;
	glm::mat4 view_;
	glm::vec3 light_position_;

	float modelx_;
	float modely_;
	float modelz_;
};

} // namespace engine

#endif // __SCENE_HPP_INCLUDED__
