// camera class providing look at and first person

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:

public:
	enum CameraType
	{
		lookat,
		firstperson
	};
	CameraType type = CameraType::lookat;

	glm::vec3 rotation = glm::vec3();
	glm::vec3 position = glm::vec3();
	glm::vec4 viewPos = glm::vec4();

	float rotationSpeed = 1.0f;
	float movementSpeed = 1.0f;

	bool updated = true;
	bool flipY = false;

	struct 
	{
		glm::mat4 perspective;
		glm::mat4 view;
	} matrices;

	struct 
	{
		bool left = false;
		bool right = false;
		bool up = false;
		bool down = false;
	} keys;

	bool moving() const
	{
		return keys.left || keys.right || keys.up || keys.down;
	}

	float getNearClip() const
	{
		return znear;
	}

	float getFarClip() const
	{
		return zfar;
	}

	void setPerspective(float fov, float aspect, float znear, float zfar)
	{
		glm::mat4 currentMatrix = matrices.perspective;
		this->fov = fov;
		this->znear = znear;
		this->zfar = zfar;
		matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
		if(flipY)
		{
			matrices.perspective[1][1] *= -1.0f;
		}
		if(matrices.view != currentMatrix)
		{
			updated = true;
		}
	}

	void updateAspectRatio(float aspect)
	{
		glm::mat4 currentMatrix = matrices.perspective;
		matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
		matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
		if (flipY)
		{
			matrices.perspective[1][1] *= -1.0f;
		}
		if (matrices.view != currentMatrix)
		{
			updated = true;
		}
	}

	void setPosition(glm::vec3 position)
	{
		this->position = position;
		updateViewMatrix();
	}

	void setRotation(glm::vec3 rotation)
	{
		this->rotation = rotation;
		updateViewMatrix();
	}

	void rotate(glm::vec3 delta)
	{
		rotation += delta;
		updateViewMatrix();
	}

	void setTranslation(glm::vec3 translation)
	{
		this->position = translation;
		updateViewMatrix();
	}

	void translate(glm::vec3 delta)
	{
		this->position += delta;
		updateViewMatrix();
	}

	void setRotationSpeed(float rotationSpeed)
	{
		this->rotationSpeed = rotationSpeed;
	}

	void setMovementSpeed(float movementSpeed)
	{
		this->movementSpeed = movementSpeed;
	}

	void update(float deltaTime)
	{
		updated = false;
		if(type == CameraType::firstperson)
		{
			if(moving())
			{
				glm::vec3 cameraFront;
				cameraFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
				cameraFront.y = sin(glm::radians(rotation.x));
				cameraFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));

				float moveSpeed = deltaTime * movementSpeed;

				if(keys.up)
				{
					position += cameraFront * moveSpeed;
				}
				if (keys.up)
				{
					position -= cameraFront * moveSpeed;
				}
				if(keys.left)
				{
					position -= glm::normalize(glm::cross(cameraFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
				}
				if(keys.right)
				{
					position += glm::normalize(glm::cross(cameraFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
				}
			}
		}
		updateViewMatrix();
	}

	bool updatePad(glm::vec2 axisLeft, glm::vec2 axisRight, float deltaTime)
	{
		bool retVal = false;
		if(type == CameraType::firstperson)
		{
			// console pattern, left joystick = view, right = move
			const float deadZone = 0.0015f;
			const float range = 1.0f - deadZone;

			glm::vec3 cameraFront;
			cameraFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
			cameraFront.y = sin(glm::radians(rotation.x));
			cameraFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));

			float moveSpeed = deltaTime * movementSpeed * 2.0f;
			float rotSpeed = deltaTime * rotationSpeed * 50.0f;


			// move
			if(fabs(axisLeft.y) > deadZone)
			{
				float pos = (fabsf(axisLeft.y) - deadZone) / range;
				position -= cameraFront * pos * ((axisLeft.y < 0.0f) ? -1.0f : 1.0f) * moveSpeed;;
				retVal = true;
			}
			if (fabs(axisLeft.x) > deadZone)
			{
				float pos = (fabsf(axisLeft.x) - deadZone) / range;
				position += glm::normalize(glm::cross(cameraFront, glm::vec3(0.0f, 1.0f, 0.0f))) * pos * ((axisLeft.x < 0.0f) ? -1.0f : 1.0f) * moveSpeed;
				retVal = true;
			}

			if(fabsf(axisRight.x) > deadZone)
			{
				float pos = (fabsf(axisRight.x) - deadZone) / range;
				rotation.y += pos * ((axisRight.x < 0.0f) ? -1.0f : 1.0f) * rotSpeed;
				retVal = true;
			}

			if (fabsf(axisRight.y) > deadZone)
			{
				float pos = (fabsf(axisRight.y) - deadZone) / range;
				rotation.x -= pos * ((axisRight.y < 0.0f) ? -1.0f : 1.0f) * rotSpeed;
				retVal = true;
			}
		}

		if(retVal)
		{
			updateViewMatrix();
		}

		return retVal;
	}

private:
	void updateViewMatrix()
	{
		glm::mat4 currentMatrix = matrices.view;

		glm::mat4 rotationMatrix = glm::mat4(1.0f);
		glm::mat4 translationMatrix;

		rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotation.x * (flipY ? -1.0f : 1.0f)), glm::vec3(1.0f, 0.0f, 0.0f));
		rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		glm::vec3 translation = position;
		if(flipY)
		{
			translation.y *= -1.0f;
		}
		translationMatrix = glm::translate(glm::mat4(1.0f), translation);

		if(type == CameraType::firstperson)
		{
			matrices.view = rotationMatrix * translationMatrix;
		}
		else 
		{
			matrices.view = translationMatrix * rotationMatrix;
		}

		viewPos = glm::vec4(position, 0.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);

		if(matrices.view != currentMatrix)
		{
			updated = true;
		}

	}
private:
	float fov{ 60.0f };
	float znear{ 0.1f };
	float zfar{ 1024.0f };

};