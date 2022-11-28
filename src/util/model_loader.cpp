#include "util/model_loader.hpp"

#include "log.hpp"

#include "resources/texture.hpp"
#include "resources/mesh.hpp"

#include "components/mesh_renderer.hpp"

#include "resource_manager.hpp"

#include <assimp/Importer.hpp>
#include <assimp/LogStream.hpp>
#include <assimp/Logger.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/postprocess.h>
#include <assimp/mesh.h>
#include <assimp/scene.h>

#include <glm/gtc/quaternion.hpp>


#include <map>

namespace engine::util {

	static void buildGraph(
		const std::map<int, std::shared_ptr<resources::Texture>>& textures,
		const std::vector<std::shared_ptr<resources::Mesh>>& meshes,
		const std::vector<unsigned int>& meshTextureIndices,
		aiNode* parentNode, Object* parentObj)
	{

		// convert to glm column major
		glm::mat4 transform;
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				transform[i][j] = parentNode->mTransformation[j][i];
			}
		}
		
		// get position
		glm::vec3 position{ transform[3][0], transform[3][1], transform[3][2] };

		// remove position from matrix
		transform[3][0] = 0.0f;
		transform[3][1] = 0.0f;
		transform[3][2] = 0.0f;

		// get scale
		glm::vec3 scale{};
		scale.x = sqrt(transform[0][0] * transform[0][0] + transform[0][1] * transform[0][1] + transform[0][2] * transform[0][2]);
		scale.y = sqrt(transform[1][0] * transform[1][0] + transform[1][1] * transform[1][1] + transform[1][2] * transform[1][2]);
		scale.z = sqrt(transform[2][0] * transform[2][0] + transform[2][1] * transform[2][1] + transform[2][2] * transform[2][2]);

		// remove scaling from matrix
		for (int row = 0; row < 3; row++) {
			transform[0][row] /= scale.x;
			transform[1][row] /= scale.y;
			transform[2][row] /= scale.z;
		}

		// get rotation
		glm::quat rotation = glm::quat_cast(transform);

		// update position, scale, rotation
		parentObj->transform.position = position;
		parentObj->transform.scale = scale;
		parentObj->transform.rotation = rotation;

		for (int i = 0; i < parentNode->mNumMeshes; i++) {
			// create child node for each mesh
			auto child = parentObj->createChild("_mesh" + std::to_string(i));
			auto childRenderer = child->createComponent<components::Renderer>();
			childRenderer->m_mesh = meshes[parentNode->mMeshes[i]];
			if (textures.contains(meshTextureIndices[parentNode->mMeshes[i]])) {
				childRenderer->m_texture = textures.at(meshTextureIndices[parentNode->mMeshes[i]]);
			}
		}

		for (int i = 0; i < parentNode->mNumChildren; i++) {
			buildGraph(textures, meshes, meshTextureIndices, parentNode->mChildren[i], parentObj->createChild("child" + std::to_string(i)));
		}
	}

	Object* loadAssimpMeshFromFile(Object* parent, const std::string& path)
	{
		Assimp::Importer importer;

		class myStream : public Assimp::LogStream {
		public:
			void write(const char* message) override {
				DEBUG("ASSIMP: {}", message);
			}
		};

		const unsigned int severity = Assimp::Logger::Debugging | Assimp::Logger::Info | Assimp::Logger::Err | Assimp::Logger::Warn;
		Assimp::DefaultLogger::get()->attachStream(new myStream, severity);

		// remove everything but texcoords, normals, meshes, materials
		importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS,
			aiComponent_ANIMATIONS |
			aiComponent_BONEWEIGHTS |
			aiComponent_CAMERAS |
			aiComponent_COLORS |
			aiComponent_LIGHTS |
			aiComponent_TANGENTS_AND_BITANGENTS |
			aiComponent_TEXTURES |
			0
		);
		importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE,
			aiPrimitiveType_POINT |
			aiPrimitiveType_LINE |
			aiPrimitiveType_POLYGON
		);

		const aiScene* scene = importer.ReadFile(path,
			aiProcess_JoinIdenticalVertices |
			aiProcess_Triangulate |
			aiProcess_SortByPType |
			aiProcess_RemoveComponent |
			aiProcess_SplitLargeMeshes | // leave at default maximum
			aiProcess_ValidateDataStructure | // make sure to log the output
			aiProcess_ImproveCacheLocality |
			aiProcess_RemoveRedundantMaterials |
			aiProcess_FindInvalidData |
			aiProcess_GenSmoothNormals |
			aiProcess_GenUVCoords |
			aiProcess_TransformUVCoords |
			aiProcess_FlipUVs | // Maybe?
			0
		);

		const char* errString = importer.GetErrorString();
		if (errString[0] != '\0' || scene == nullptr) {
			throw std::runtime_error(errString);
		}

		if (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
			throw std::runtime_error(errString);
		}
		
		assert(scene->HasAnimations() == false);
		assert(scene->HasCameras() == false);
		assert(scene->HasLights() == false);
		assert(scene->hasSkeletons() == false);

		INFO("material count: {}, mesh count: {}", scene->mNumMaterials, scene->mNumMeshes);

		std::map<int, std::shared_ptr<resources::Texture>> textures{};

		for (int i = 0; i < scene->mNumMaterials; i++) {
			const aiMaterial* m = scene->mMaterials[i];
			INFO("Material {}:", i);
			INFO("  Name: {}", m->GetName().C_Str());
			for (int j = 0; j < m->mNumProperties; j++) {
				const aiMaterialProperty* p = m->mProperties[j];
				INFO("  prop {}, key: {}", j, p->mKey.C_Str());
			}
			
			if (aiGetMaterialTextureCount(m, aiTextureType_DIFFUSE) >= 1) {
				aiString texPath{};
				aiGetMaterialTexture(m, aiTextureType_DIFFUSE, 0, &texPath);
				INFO("  Diffuse tex: {}", texPath.C_Str());
				std::filesystem::path absPath = path;
				absPath = absPath.parent_path();
				absPath /= texPath.C_Str();
				try {
					textures[i] = std::make_shared<resources::Texture>(absPath);
				} catch (const std::runtime_error& e) {
					textures[i] = parent->res.get<resources::Texture>("textures/white.png");
				}
			}
		}

		std::vector<std::shared_ptr<resources::Mesh>> meshes{};
		std::vector<unsigned int> meshMaterialIndices{};
		for (int i = 0; i < scene->mNumMeshes; i++) {
			const aiMesh* m = scene->mMeshes[i];
			meshMaterialIndices.push_back(m->mMaterialIndex);
			std::vector<Vertex> vertices(m->mNumVertices);
			std::vector<uint32_t> indices(m->mNumFaces * 3);
			INFO("Mesh {}: vertex count {}", i, vertices.size());
			INFO("Mesh {}: index count {}", i, indices.size());

			for (int j = 0; j < vertices.size(); j++) {
				Vertex v{};
				v.pos.x = m->mVertices[j].x;
				v.pos.y = m->mVertices[j].y;
				v.pos.z = m->mVertices[j].z;
				v.norm.x = m->mNormals[j].x;
				v.norm.y = m->mNormals[j].y;
				v.norm.z = m->mNormals[j].z;
				vertices[j] = v;
			}
			if (m->mNumUVComponents[0] >= 2) {
				for (int j = 0; j < vertices.size(); j++) {
					vertices[j].uv.x = m->mTextureCoords[0][j].x;
					vertices[j].uv.y = m->mTextureCoords[0][j].y;

				}
			}
		
			for (int j = 0; j < indices.size() / 3; j++) {
				indices[j * 3 + 0] = m->mFaces[j].mIndices[0];
				indices[j * 3 + 1] = m->mFaces[j].mIndices[1];
				indices[j * 3 + 2] = m->mFaces[j].mIndices[2];
			}
			meshes.push_back(std::make_shared<resources::Mesh>(vertices, indices));
		}

		Object* obj = parent->createChild(scene->GetShortFilename(path.c_str()));

		buildGraph(textures, meshes, meshMaterialIndices, scene->mRootNode, obj);

		Assimp::DefaultLogger::kill();
		return obj;
	}

}
