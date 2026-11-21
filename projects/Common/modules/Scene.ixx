module;
#include <glm/mat4x4.hpp>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
export module Scene;

export import Camera;
export import ObjectId;
import Input;
export import Lighting;
import SettingsRenderer;

export namespace gg
{
	/* A model in the scene: which file, drawn with which shader, and its model-to-world transform.
	   Scene::AddModel creates one with a new id. */
	class SceneModel
	{
	public:
		SceneModel(ObjectId objectId, std::string modelPath, std::string shader, glm::mat4x4 const& modelTransform)
			: transform{ modelTransform }
			, id{ objectId }
			, modelRelativePath{ std::move(modelPath) }
			, shaderName{ std::move(shader) }
		{
		}

		ObjectId           GetId() const { return id; }
		std::string const& GetModelRelativePath() const { return modelRelativePath; }
		std::string const& GetShaderName() const { return shaderName; }

		glm::mat4x4 transform{ 1.f }; /* game logic moves the model */

	private:
		ObjectId    id;
		std::string modelRelativePath;
		std::string shaderName;
	};

	/* The scene's sky: the cube map set to load and the shader that draws it. Scene::SetSkybox creates one
	   with a new id. */
	class SceneSkybox
	{
	public:
		SceneSkybox(ObjectId objectId, std::string skyboxName, std::string shader)
			: id{ objectId }
			, name{ std::move(skyboxName) }
			, shaderName{ std::move(shader) }
		{
		}

		ObjectId           GetId() const { return id; }
		std::string const& GetName() const { return name; }
		std::string const& GetShaderName() const { return shaderName; }

	private:
		ObjectId    id;
		std::string name;
		std::string shaderName;
	};

	/* What is being shown, independent of how it is drawn. Owned by the Application and passed to the
	   renderer each frame. Objects added through the scene get a new id from its counter; an object's id
	   cannot be changed afterwards. */
	class Scene
	{
	public:
		explicit Scene(std::shared_ptr<InputManager> inputManager)
			: camera{ NewObjectId(), std::move(inputManager) }
		{
		}

		ObjectId AddModel(std::string modelRelativePath, std::string shaderName, glm::mat4x4 const& transform)
		{
			models.push_back(SceneModel{ NewObjectId(), std::move(modelRelativePath), std::move(shaderName), transform });
			return models.back().GetId();
		}

		/* Replaces the current skybox; the new one gets a new id */
		ObjectId SetSkybox(std::string name, std::string shaderName)
		{
			skybox = SceneSkybox{ NewObjectId(), std::move(name), std::move(shaderName) };
			return skybox->GetId();
		}

		std::span<SceneModel const>       GetModels() const { return models; }
		std::span<SceneModel>             GetModels() { return models; } /* to move models; ids stay read-only */
		std::optional<SceneSkybox> const& GetSkybox() const { return skybox; }
		Camera const&                     GetCamera() const { return camera; }
		Camera&                           GetCamera() { return camera; }

		DirectionalLight light{ defaultDirectionalLight };

	private:
		ObjectId NewObjectId() { return nextObjectId++; }

		/* Declared before the camera: the camera's id is taken from it during construction */
		ObjectId                   nextObjectId{ 1 };
		Camera                     camera;
		std::vector<SceneModel>    models;
		std::optional<SceneSkybox> skybox;
	};
} // namespace gg
