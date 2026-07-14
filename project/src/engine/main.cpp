#include <Windows.h>
#include "cstdint"
#include <string>
#include <fstream>
#include <sstream>
#include <chrono>
#include <format>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include "dbghelp.h"
#include "strsafe.h"
#include <dxgidebug.h>
#include <dxcapi.h>
#include <Vector>
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/DirectXTex/DirectXTex.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include "externals/DirectXTex/d3dx12.h"
#include <numbers>
#include <wrl.h>
#include <xaudio2.h>
#include <direct.h>

#include "Vector.h"
#include "Matrix.h"
#include "MathFunc.h"
#include "Input.h"
#include "Window.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "D3DResourceLeakChecker.h"
#include "SpriteCommon.h"
#include "Sprite.h"
#include "TextureManager.h"
#include "ModelCommon.h"
#include "Model.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "ModelManager.h"
#include "SrvManager.h"
#include "SkyBox.h"
#include "ParticleManager.h"
#include "Ring.h"
#include "Cylinder.h"
#include "KeyframeAnimation.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "xaudio2.lib")


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception);

struct Sphere {
	Vector3 center;
	float radius;
};

struct ChunkHeader {
	char id[4];
	int32_t size;
};

struct RiffHeader {
	ChunkHeader chunk;
	char type[4];
};

struct FormatChunk {
	ChunkHeader chunk;
	WAVEFORMATEX fmt;
};

struct SoundData {
	WAVEFORMATEX wfex;
	BYTE* pBuffer;
	unsigned int bufferSize;
};

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	//ポインタ
	Input* input = nullptr;
	Window* window = nullptr;
	DirectXCommon* dxCommon = nullptr;
	SpriteCommon* spriteCommon = nullptr;
	Sprite* sprite = nullptr;
	ModelCommon* modelCommon = nullptr;
	Model* model = nullptr;
	Model* terrainModel = nullptr;
	Object3dCommon* object3dCommon = nullptr;
	Object3d* object3d = nullptr;
	Object3d* object3d2 = nullptr;
	SrvManager* srvManager = nullptr;
	SkyBox* skyBox = nullptr;
	ParticleManager* particleManager = nullptr;
	ParticleManager* particleManager2 = nullptr;
	ParticleManager* smokeManager = nullptr;
	ParticleManager* flashManager = nullptr;
	Ring* ring = nullptr;
	Cylinder* cylinder = nullptr;
	KeyframeAnimation* keyframeAnimation = nullptr;
	Model::Skeleton skeleton;

	//初期化
	window = new Window();
	dxCommon = new DirectXCommon();
	spriteCommon = new SpriteCommon();
	sprite = new Sprite();
	modelCommon = new ModelCommon();
	terrainModel = new Model();
	model = new Model();
	object3dCommon = new Object3dCommon();
	object3d = new Object3d();
	object3d2 = new Object3d();
	Camera* camera = new Camera();
	srvManager = new SrvManager();
	skyBox = new SkyBox();
	particleManager = new ParticleManager();
	particleManager2 = new ParticleManager();
	smokeManager = new ParticleManager();
	flashManager = new ParticleManager();
	ring = new Ring();
	cylinder = new Cylinder();
	keyframeAnimation = new KeyframeAnimation();

	
	ID3D12Resource* CreateDepthStencilTextureResource(ID3D12Device * device, int32_t width, int32_t height);
	void CreateSphereVertices(int kSubdivision, float radius, std::vector<VertexData>&vertexData);
	
	////変数の宣言
	//HRESULT hr;
	Transform transform{ {1.0f,1.0f,1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	Transform transformSprite{ {1.0f,1.0f,1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	Transform cameraTransform{ {0.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f,}, {0.0f,0.0f, -5.0f} };
	Sphere sphere = { {0.0f, 0.0f, 0.0f}, 1.0f };
	Transform uvTransformSprite{ {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f}, };
	//std::vector<VertexData> sphereVertices;
	//Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, static_cast<float>(Window::kClientWidth) / static_cast<float>(Window::kClientHeight), 0.1f, 100.0f);
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
	Matrix4x4 viewMatrix = Inverse(cameraMatrix);
	//Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
	//transformationMatrixData = worldViewProjectionMatrix;

	//スプライトの変換行列
	Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
	Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
	Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, static_cast<float>(Window::kClientWidth), static_cast<float>(Window::kClientHeight), 0.0f, 100.0f);
	Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));

	//球の変換行列
	//Matrix4x4 worldMatrixSphere = MakeAffineMatrix(
	//	{ sphere.radius, sphere.radius, sphere.radius }, // スケール
	//	{ 0.0f, 0.0f, 0.0f },                           // 回転
	//	sphere.center                                   // 平行移動
	//);
	//Matrix4x4 worldViewProjectionMatrixSphere = Multiply(worldMatrixSphere, Multiply(viewMatrix, projectionMatrix));

	//UVTransform用の行列
	Matrix4x4 uvTransformMatrix = MakeAffineMatrix(uvTransformSprite.scale, uvTransformSprite.rotate, uvTransformSprite.translate);

	//Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
	//IXAudio2MasteringVoice* masterVoice;
	//HRESULT result;

	window->Initialize();
	dxCommon->Initialize(window);
	srvManager->Initialize(dxCommon);
	TextureManager::GetInstance()->Initialize(dxCommon, srvManager);
	spriteCommon->Initialize(dxCommon);
	sprite->Initialize(spriteCommon, "resources/uvChecker.png");
	ModelManager::GetInstance()->Initialize(dxCommon);
	//ModelManager::GetInstance()->LoadModel("walk.gltf"); //.objからモデルを読み込む
	modelCommon->Initialize(dxCommon);
	model->initialize(modelCommon, "resources", "walk.gltf");
	skeleton = model->CreateSkeleton(model->GetRootNode());
	terrainModel->initialize(modelCommon, "resources", "terrain.obj");
	object3dCommon->Initialize(dxCommon);

	camera->SetRotate({ 0.3f,0.0f,0.0f });
	//camera->SetTranslate({ 0.0f,0.0f,0.0f });
	object3d->Initialize(object3dCommon);
	object3d->SetModel(model);
	object3d->SetCamera(camera);
	object3d2->Initialize(object3dCommon);
	object3d2->SetModel(terrainModel);
	object3d2->SetCamera(camera);
	object3d2->SetTranslate({ 0.0f, -2.0f, 0.0f });

	skyBox->Initialize(dxCommon);
	ring->Initialize(dxCommon);
	cylinder->Initialize(dxCommon);

	particleManager->Initialize(dxCommon, srvManager, camera, "resources/circle.png");
	particleManager->SetSpeed(0.0f);
	particleManager2->Initialize(dxCommon, srvManager, camera, "resources/gradationLine.png", ParticleManager::PrimitiveType::Ring);
	particleManager2->SetEmitCount(1);
	particleManager2->SetScale(0.5f);
	particleManager2->SetLength(0.5f);
	particleManager2->SetSpeed(0.0f);
	particleManager2->SetScaleVelocity(32.0f);
	smokeManager->Initialize(dxCommon, srvManager, camera, "resources/circle.png", ParticleManager::PrimitiveType::Plane, ParticleManager::BlendMode::Alpha);
	smokeManager->SetPlaneSize(0.5f, 0.5f);
	smokeManager->SetEmitCount(10);
	smokeManager->SetColor({ 0.3f, 0.3f, 0.3f, 0.7f });
	smokeManager->SetLifeTimeRange(2.0f, 3.5f);
	smokeManager->SetSpeedRange(0.2f, 0.8f);
	smokeManager->SetUniformScaleRange(0.8f, 1.4f);
	smokeManager->SetScaleVelocityRange(1.0f, 16.0f);
	flashManager->Initialize(dxCommon, srvManager, camera, "resources/circle.png");
	flashManager->SetPlaneSize(0.5f, 0.5f);
	flashManager->SetEmitCount(1);
	flashManager->SetColor({ 1.0f, 0.75f, 0.25f, 1.0f });
	flashManager->SetLifeTime(0.3f);
	flashManager->SetSpeed(0.0f);
	flashManager->SetUniformScaleRange(8.0f, 8.0f);
	flashManager->SetScaleVelocity(8.0f);

	//camera->SetRotate({ 0.0f,0.0f,0.0f });
	//camera->SetTranslate({ 0.0f,0.0f,0.0f });
	//object3dCommon->SetDefaultCamera(camera);

	//result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	//result = xAudio2->CreateMasteringVoice(&masterVoice);

#ifdef _DEBUG

	ID3D12Debug1* debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();

		debugController->SetEnableGPUBasedValidation(TRUE);
	}

	//エラーや警告を出す
	ID3D12InfoQueue* infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {

		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		D3D12_MESSAGE_ID denyIds[] = {
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};

		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		infoQueue->PushStorageFilter(&filter);

		infoQueue->Release();
	}

#endif

	//入力の初期化
	input = new Input();
	input->Initialize(window);

	float YRotateSpeed = 0;
	float flashLightTime = 0.0f;
	Vector3 debugCameraRotate = camera->GetRotate();
	Vector3 debugCameraTranslate = camera->GetTranslate();
	const float mouseRotateSensitivity = 0.002f;
	const float mouseWheelSensitivity = 0.01f;

	//メインループ
	MSG msg{};
	//ウィンドウの×ボタンが押されるまでループ
	while (msg.message != WM_QUIT) {

		//Windowsのメッセージ処理
		if (window->ProcessMessage()) {
			break;
		}

		//ゲームの処理
		//入力の更新
		input->Update();

		if (input->PushMouse(1)) {
			debugCameraRotate.x += static_cast<float>(input->GetMouseMoveY()) * mouseRotateSensitivity;
			debugCameraRotate.y += static_cast<float>(input->GetMouseMoveX()) * mouseRotateSensitivity;
			debugCameraRotate.x = std::clamp(debugCameraRotate.x, -1.45f, 1.45f);
		}

		if (input->GetMouseWheel() != 0) {
			debugCameraTranslate.z += static_cast<float>(input->GetMouseWheel()) * mouseWheelSensitivity;
		}

		camera->SetRotate(debugCameraRotate);
		camera->SetTranslate(debugCameraTranslate);

		cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
		viewMatrix = Inverse(cameraMatrix);

		//YRotateSpeed += 0.01f;
		//object3d->SetRotate({ 0.0f, YRotateSpeed, 0.0f });
		//object3d2->SetRotate({ 0.0f, YRotateSpeed, 0.0f });
		worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
		//worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
		//wvpData->WVP = worldViewProjectionMatrix;
		//wvpData->World = worldMatrix;
		//*wvpData = worldViewProjectionMatrix;

		worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
		worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));
		//*transformationMatrixDataSprite = worldViewProjectionMatrixSprite;

		uvTransformMatrix = MakeAffineMatrix(uvTransformSprite.scale, uvTransformSprite.rotate, uvTransformSprite.translate);
		//materialDataSprite->uvTransform = uvTransformMatrix;

		if (input->TriggerKey(DIK_0)) {
			OutputDebugStringA("Hit 0\n");
		}

		if (input->TriggerKey(DIK_SPACE)) {
			particleManager->Emit({ 0.0f, 0.0f, 0.0f });
			particleManager2->Emit({ 0.0f, 0.0f, 0.0f });
			smokeManager->Emit({ 0.0f, -1.7f, 0.0f });
			flashManager->Emit({ 0.0f, 0.0f, 0.0f });
			flashLightTime = 0.15f;
			//std::string message = "Particle count: " + std::to_string(particleManager->GetParticleCount()) + "\n";
			//OutputDebugStringA(message.c_str());
		}

		particleManager->Update(1.0f / 60.0f);
		particleManager2->Update(1.0f / 60.0f);
		smokeManager->Update(1.0f / 60.0f);
		flashManager->Update(1.0f / 60.0f);
		const float flashLightIntensity = 8.0f * (flashLightTime / 0.15f);
		object3d2->SetPointLight({ 0.0f, 0.0f, 0.0f }, 
			{ 1.0f, 0.75f, 0.25f, 1.0f }, 
			flashLightIntensity, 14.0f, 2.0f);
		if (flashLightTime > 0.0f) {
			flashLightTime -= 1.0f / 60.0f;
			if (flashLightTime < 0.0f) {
				flashLightTime = 0.0f;
			}
		}

		/*ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGui::ShowDemoWindow();
		ImGui::DragFloat3("cameraScale", &cameraTransform.scale.x, 0.01f);
		ImGui::DragFloat3("cameraRotate", &cameraTransform.rotate.x, 0.01f);
		ImGui::DragFloat3("cameraTranslate", &cameraTransform.translate.x, 0.01f);
		ImGui::DragFloat3("transform", &transform.translate.x, 0.01f);
		ImGui::DragFloat2("transformSprite", &transformSprite.translate.x, 1.0f);*/
		//ImGui::DragFloat3("Light", &directionalLightData->direction.x, 0.01f);
		//ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 1.0f);
		//ImGui::ColorEdit3("Triangle Color", triangleColor);
		////ImGui::Checkbox("useMonsterBall", &useMonsterBall);
		//*materialData = Material{ Vector4{triangleColor[0], triangleColor[1], triangleColor[2], 0.0f}, 1 };
		//*materialData = Material{ Vector4{triangleColor[0], triangleColor[1], triangleColor[2], 1.0f}, 1, {0,0,0}, MakeIdentity4x4() };
		////*materialData = Vector4(triangleColor[0], triangleColor[1], triangleColor[2], 1.0f);

		/*{
			Vector3& dir = directionalLightData->direction;
			float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
			if (len > 0.0001f) {
				dir.x /= len;
				dir.y /= len;
				dir.z /= len;
			}
		}*/

		//if (fence->GetCompletedValue() < fenceValue) {
		//	fence->SetEventOnCompletion(fenceValue, fenceEvent);
		//	WaitForSingleObject(fenceEvent, INFINITE);
		//}

		//hr = commandAllocator->Reset();
		//assert(SUCCEEDED(hr));
		//hr = commandList->Reset(commandAllocator, nullptr);
		//assert(SUCCEEDED(hr));

		//UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

		

		srvManager->PreDraw();
		dxCommon->PreDraw();

		/*spriteCommon->CreatePrimitiveTopology();
		sprite->Update();
		sprite->Draw();*/

		camera->Update();
		keyframeAnimation->Update(1.0f / 60.0f);
		keyframeAnimation->ApplyAnimation(skeleton, keyframeAnimation->GetAnimation(), keyframeAnimation->GetAnimationTime());
		model->Update(skeleton);
		object3d->Update();
		object3d2->Update();

		/*skyBox->Update(camera);
		skyBox->CreatePrimitiveTopology();
		skyBox->Draw();*/

		/*ring->Update(camera);
		ring->CreatePrimitiveTopology();
		ring->Draw();*/

		cylinder->Update(camera);
		cylinder->CreatePrimitiveTopology();
		cylinder->Draw();

		object3dCommon->CreatePrimitiveTopology();
		object3d->Draw();
		object3d2->Draw();
		model->DrawSkeleton(skeleton, object3d->GetWorldMatrix(), camera);

		particleManager->Draw();
		particleManager2->Draw();
		flashManager->Draw();
		smokeManager->Draw();
		

		dxCommon->PostDraw();
		//TextureManager::GetInstance()->Finalize();

	}

#ifdef _DEBUG

	debugController->Release();

#endif

	//リソースリークチェック
	IDXGIDebug1* debug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		debug->Release();
	}

	window->Finalize();

	TextureManager::GetInstance()->Finalize();
	ModelManager::GetInstance()->Finalize();
	delete input;
	delete particleManager;
	delete particleManager2;
	delete smokeManager;
	delete flashManager;
	delete keyframeAnimation;
	delete srvManager;
	delete window;
	delete dxCommon;

	return 0;
}

static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };
	CreateDirectory(L"./Dumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
	HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();

	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;

	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);

	return EXCEPTION_EXECUTE_HANDLER;
}

Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
	Matrix4x4 result;

	result.m[0][0] = width / 2.0f;
	result.m[0][1] = 0.0f;
	result.m[0][2] = 0.0f;
	result.m[0][3] = 0.0f;

	result.m[1][0] = 0.0f;
	result.m[1][1] = -(height / 2.0f);
	result.m[1][2] = 0.0f;
	result.m[1][3] = 0.0f;

	result.m[2][0] = 0.0f;
	result.m[2][1] = 0.0f;
	result.m[2][2] = maxDepth - minDepth;
	result.m[2][3] = 0.0f;

	result.m[3][0] = left + width / 2.0f;
	result.m[3][1] = top + height / 2.0f;
	result.m[3][2] = minDepth;
	result.m[3][3] = 1.0f;

	return result;
}

ID3D12Resource* CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height) {
	//生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	//利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	//深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	//Resourceの生成
	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));

	return resource;
}

D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}


