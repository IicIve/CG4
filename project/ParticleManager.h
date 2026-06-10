#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <random>
#include <string>
#include <wrl.h>
#include <d3d12.h>

#include "Matrix.h"
#include "Particle.h"
#include "Vector.h"

class Camera;
class DirectXCommon;
class SrvManager;

class ParticleManager {
public:
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, Camera* camera, const std::string& textureFilePath);
	void Update(float deltaTime);
	void Draw();
	void Emit(const Vector3& position);
	Particle MakeNewParticle(std::mt19937& randomEngine);

	void SetRotate(float rotate);
	void SetRotateRange(float minRotate, float maxRotate);
	void SetRotateVelocity(float rotateVelocity);
	void SetRotateVelocityRange(float minRotateVelocity, float maxRotateVelocity);
	void SetLength(float length);
	void SetLengthRange(float minLength, float maxLength);

	std::size_t GetParticleCount() const { return particles_.size(); }

private:
	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
	};

	struct ViewProjection {
		Matrix::Matrix4x4 viewProjection;
	};

	struct ParticleForGPU {
		Vector3 translate;
		float scaleX;
		Vector4 color;
		float rotate;
		float length;
		float padding[2];
	};

	void CreateVertexResource();
	void CreateParticleResource();
	void CreateRootSignature();
	void CreateGraphicsPipelineState();

	static const uint32_t kMaxParticleCount = 1024;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Camera* camera_ = nullptr;
	std::string textureFilePath_;

	std::random_device seedGenerator_;
	std::mt19937 randomEngine_{ seedGenerator_() };
	float minRotate_ = -3.14159265f;
	float maxRotate_ = 3.14159265f;
	float minRotateVelocity_ = 0.0f;
	float maxRotateVelocity_ = 0.0f;
	float minLength_ = 2.0f;
	float maxLength_ = 8.0f;

	std::list<Particle> particles_;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	VertexData* vertexData_ = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> viewProjectionResource_;
	ViewProjection* viewProjectionData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> particleResource_;
	ParticleForGPU* particleData_ = nullptr;
	uint32_t particleSrvIndex_ = 0;
	uint32_t particleDrawCount_ = 0;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
};
