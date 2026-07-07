
#include "Model.h"
#include "TextureManager.h"
#include "Logger.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <fstream>
#include <sstream>
#include <cassert>

void Model::initialize(ModelCommon* modelCommon, const std::string& directorypath, const std::string& filename) {
	this->modelCommon_ = modelCommon;

	modelData = LoadModelFile(directorypath, filename);

	//モデルの頂点リソースを作る
	vertexResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());
	//頂点バッファビューを作る
	//D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	vertexBufferView.StrideInBytes = sizeof(VertexData);
	//頂点リソースにデータを書き込む
	vertexData = nullptr;
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

	//マテリアル用のリソース作成
	materialResource = modelCommon_->GetDxCommon()->CreateBufferResource(256);
	materialData = nullptr;
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	float triangleColor[3] = { 1.0f, 1.0f, 1.0f };
	materialData->color = Vector4(triangleColor[0], triangleColor[1], triangleColor[2], 1.0f);
	materialData->enableLighting = true;
	materialData->uvTransform = MakeIdentity4x4();

	//.objの参照しているテクスチャファイル読み込み
	TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);
	//読み込んだテクスチャの番号を取得
	modelData.material.textureIndex =
		TextureManager::GetInstance()->GetSrvIndex(modelData.material.textureFilePath);


}

void Model::Draw() {
	modelCommon_->GetDxCommon()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
	modelCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
	modelCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(modelData.material.textureFilePath));
	modelCommon_->GetDxCommon()->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);
}

Model::MaterialData Model::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
	Model::MaterialData materialData;
	std::string line;
	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}
	return materialData;
	
}

Model::ModelData Model::LoadModelFile(const std::string& directoryPath, const std::string& filename) {
	Model::ModelData modelData;
	std::vector<Vector4> positions;
	std::vector<Vector3> normals;
	std::vector<Vector2> texcoords;
	std::string line;
	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_Triangulate);
	if (scene == nullptr) {
		Logger::Log(std::string("Assimp import failed: ") + importer.GetErrorString() + "\n");
		assert(scene);
	}
	assert(scene->HasMeshes());

	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		aiMesh* mesh = scene->mMeshes[meshIndex];
		assert(mesh->HasNormals());//法線がないメッシュは非対応
		assert(mesh->HasTextureCoords(0));//テクスチャ座標がないメッシュは非対応

		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
			aiFace& face = mesh->mFaces[faceIndex];
			assert(face.mNumIndices == 3);//三角形のみサポート

			for (uint32_t element = 0; element < face.mNumIndices; ++element) {
				uint32_t vertexIndex = face.mIndices[element];
				aiVector3D& position = mesh->mVertices[vertexIndex];
				aiVector3D& normal = mesh->mNormals[vertexIndex];
				aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];
				VertexData vertex;
				vertex.position = { position.x, position.y, position.z, 1.0f };
				vertex.normal = { normal.x, normal.y, normal.z };
				vertex.texcoord = { texcoord.x, texcoord.y };
				//aiProcess_MakeLeftHandedはz*=-1で右手->左手に変換するので手動で対処
				vertex.position.x *= -1.0f;
				vertex.normal.x *= -1.0f;
				modelData.vertices.push_back(vertex);
			}
		}
	}

	for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
		aiMaterial* material = scene->mMaterials[materialIndex];
		if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
			aiString textureFilePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
			modelData.material.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
		}
	}

	modelData.rootNode = ReadNode(scene->mRootNode);

	//while (std::getline(file, line)) {
	//	std::string identifier;
	//	std::istringstream s(line);
	//	s >> identifier;

	//	if (identifier == "v") {
	//		Vector4 position;
	//		s >> position.x >> position.y >> position.z;
	//		position.x *= -1.0f;
	//		position.w = 1.0f;
	//		positions.push_back(position);
	//	} else if (identifier == "vt") {
	//		Vector2 texcoord;
	//		s >> texcoord.x >> texcoord.y;
	//		texcoord.y = 1.0f - texcoord.y;
	//		texcoords.push_back(texcoord);
	//	} else if (identifier == "vn") {
	//		Vector3 normal;
	//		s >> normal.x >> normal.y >> normal.z;
	//		normal.x *= -1.0f;
	//		normals.push_back(normal);
	//	} else if (identifier == "f") {
	//		Model::VertexData triangle[3];
	//		//面は三角形限定
	//		for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
	//			std::string vertexDefinition;
	//			s >> vertexDefinition;
	//			//
	//			std::istringstream v(vertexDefinition);
	//			uint32_t elementIndices[3];
	//			for (uint32_t element = 0; element < 3; ++element) {
	//				std::string index;
	//				std::getline(v, index, '/');
	//				elementIndices[element] = std::stoi(index);
	//			}
	//			//頂点を構築
	//			Vector4 position = positions[elementIndices[0] - 1];
	//			Vector2 texcoord = texcoords[elementIndices[1] - 1];
	//			Vector3 normal = normals[elementIndices[2] - 1];
	//			triangle[faceVertex] = { position, texcoord, normal };
	//			//VertexData vertex = { position, texcoord, normal };
	//			//modelData.vertices.push_back(vertex);
	//		}
	//		modelData.vertices.push_back(triangle[0]);
	//		modelData.vertices.push_back(triangle[2]);
	//		modelData.vertices.push_back(triangle[1]);
	//	} else if (identifier == "mtllib") {
	//		std::string materialFilename;
	//		s >> materialFilename;
	//		modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
	//	}
	//}
	return modelData;
	
}

Model::Node Model::ReadNode(aiNode* node) {
	Node result;

	aiMatrix4x4 aiLocalMatrix = node->mTransformation;//nodeのlocalMatrixを取得
	aiLocalMatrix.Transpose();//列ベクトル形式を行ベクトル形式に変換
	result.localMatrix.m[0][0] = aiLocalMatrix[0][0];
	result.localMatrix.m[0][1] = aiLocalMatrix[0][1];
	result.localMatrix.m[0][2] = aiLocalMatrix[0][2];
	result.localMatrix.m[0][3] = aiLocalMatrix[0][3];

	result.localMatrix.m[1][0] = aiLocalMatrix[1][0];
	result.localMatrix.m[1][1] = aiLocalMatrix[1][1];
	result.localMatrix.m[1][2] = aiLocalMatrix[1][2];
	result.localMatrix.m[1][3] = aiLocalMatrix[1][3];

	result.localMatrix.m[2][0] = aiLocalMatrix[2][0];
	result.localMatrix.m[2][1] = aiLocalMatrix[2][1];
	result.localMatrix.m[2][2] = aiLocalMatrix[2][2];
	result.localMatrix.m[2][3] = aiLocalMatrix[2][3];

	result.localMatrix.m[3][0] = aiLocalMatrix[3][0];
	result.localMatrix.m[3][1] = aiLocalMatrix[3][1];
	result.localMatrix.m[3][2] = aiLocalMatrix[3][2];
	result.localMatrix.m[3][3] = aiLocalMatrix[3][3];

	result.name = node->mName.C_Str();//node名を格納
	result.children.resize(node->mNumChildren);//子ノードの数だけ確保
	for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
		//再帰的に読んで階層構造を作っていく
		result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
	}

	return result;
}
