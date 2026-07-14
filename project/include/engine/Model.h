#pragma once

#include <dxgi1_6.h>
#include <wrl.h>
#include <d3d12.h>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <optional>

#include "ModelCommon.h"
#include "Vector.h"
#include "Matrix.h"
#include "MathFunc.h"
#include "Camera.h"
#include <map>

//class ModelCommon;
struct aiNode;

using namespace Vector;
using namespace Matrix;

class Model {
public:

	//構造体

	struct QuaternionTransform {
		Vector3 translate;
		Quaternion rotate;
		Vector3 scale;
	};

	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	struct MaterialData {
		std::string textureFilePath;
		uint32_t textureIndex = 0;
	};

	struct Node {
		QuaternionTransform transform;
		Matrix4x4 localMatrix;
		std::string name;
		std::vector<Node> children;
	};

	struct ModelData {
		std::vector<VertexData> vertices;
		std::vector<uint32_t> indices;
		MaterialData material;
		Node rootNode;
	};

	struct Material {
		Vector4 color;
		int enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
	};

	struct Joint {
		QuaternionTransform transform;//Transform情報
		Matrix4x4 localMatrix;
		Matrix4x4 skeletonSpaceMatrix;
		std::string name;
		std::vector<int32_t> children;
		int32_t index;
		std::optional<int32_t> parent;
	};

	struct Skeleton {
		int32_t root;
		std::map<std::string, int32_t> jointMap;
		std::vector<Joint> joints;
	};

	struct SkeletonLineVertex {
		Vector4 position;
	};

	struct SkeletonLineTransformation {
		Matrix4x4 viewProjection;
	};

	//関数

	void initialize(ModelCommon* modelCommon, const std::string& directorypath, const std::string& filename);
	void Update(Skeleton& skeleton);
	void Draw();
	void DrawSkeleton(const Skeleton& skeleton, const Matrix4x4& worldMatrix, Camera* camera);
	static ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);
	Skeleton CreateSkeleton(const Node& rootNode);

	const Node& GetRootNode() const { return modelData.rootNode; }
	void SetRootLocalMatrix(const Matrix4x4& localMatrix) { modelData.rootNode.localMatrix = localMatrix; }

private:
	//関数

	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
	static Node ReadNode(aiNode* node);
	int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints);
	void CreateSkeletonLinePipeline();
	void CreateSkeletonLineVertexResource(size_t vertexCount);

	//変数

	ModelCommon* modelCommon_ = nullptr;
	//Objファイルのデータ
	ModelData modelData;

	//頂点データ
	//バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	//バッファリソース内のデータを指すポインタ
	VertexData* vertexData = nullptr;
	//バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};

	//マテリアルデータ
	//バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	//バッファリソース内のデータを指すポインタ
	Material* materialData = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> skeletonLineVertexResource;
	D3D12_VERTEX_BUFFER_VIEW skeletonLineVertexBufferView{};
	SkeletonLineVertex* skeletonLineVertexData = nullptr;
	size_t skeletonLineVertexCapacity = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> skeletonLineTransformationResource;
	SkeletonLineTransformation* skeletonLineTransformationData = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> skeletonLineRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> skeletonLinePipelineState = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	std::uint32_t* indexData = nullptr;

};

