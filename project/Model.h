#pragma once

#include <dxgi1_6.h>
#include <wrl.h>
#include <d3d12.h>
#include <cstdint>
#include <string>
#include <vector>

#include "ModelCommon.h"
#include "Vector.h"
#include "Matrix.h"
#include "MathFunc.h"

//class ModelCommon;

using namespace Vector;
using namespace Matrix;

class Model {
public:

	//構造体

	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	struct MaterialData {
		std::string textureFilePath;
		uint32_t textureIndex = 0;
	};

	struct ModelData {
		std::vector<VertexData> vertices;
		MaterialData material;
	};

	struct Material {
		Vector4 color;
		int enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
	};

	//関数

	void initialize(ModelCommon* modelCommon, const std::string& directorypath, const std::string& filename);
	void Draw();

private:
	//関数

	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);


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



};

