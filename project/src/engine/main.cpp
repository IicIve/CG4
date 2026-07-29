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
	ParticleManager* handParticleManager = nullptr;
	ParticleManager* leftHandParticleManager = nullptr;
	Ring* ring = nullptr;
	Cylinder* cylinder = nullptr;
	KeyframeAnimation* keyframeAnimation = nullptr;
	Model::Skeleton skeleton;
	Model::SkinCluster skinCluster;

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
	handParticleManager = new ParticleManager();
	leftHandParticleManager = new ParticleManager();
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
	skinCluster = model->CreateSkinCluster(dxCommon->GetDevice(), srvManager, skeleton, model->GetModelData());
	const auto rightHandJointIt = skeleton.jointMap.find("mixamorig:RightHand");
	const int32_t rightHandJointIndex = rightHandJointIt != skeleton.jointMap.end() ? rightHandJointIt->second : -1;
	const auto leftHandJointIt = skeleton.jointMap.find("mixamorig:LeftHand");
	const int32_t leftHandJointIndex = leftHandJointIt != skeleton.jointMap.end() ? leftHandJointIt->second : -1;
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
	handParticleManager->Initialize(dxCommon, srvManager, camera, "resources/circle.png", ParticleManager::PrimitiveType::Plane, ParticleManager::BlendMode::Add);
	handParticleManager->SetPlaneSize(0.5f, 0.5f);
	handParticleManager->SetEmitCount(1);
	handParticleManager->SetColor({ 0.05f, 1.0f, 0.05f, 0.8f });
	handParticleManager->SetLifeTimeRange(1.2f, 2.4f);
	handParticleManager->SetSpeedRange(0.0f, 0.02f);
	handParticleManager->SetUniformScaleRange(0.25f, 0.5f);
	handParticleManager->SetScaleVelocityRange(0.1f, 0.2f);
	leftHandParticleManager->Initialize(dxCommon, srvManager, camera, "resources/circle.png", ParticleManager::PrimitiveType::Plane, ParticleManager::BlendMode::Add);
	leftHandParticleManager->SetPlaneSize(0.5f, 0.5f);
	leftHandParticleManager->SetEmitCount(1);
	leftHandParticleManager->SetColor({ 1.0f, 0.05f, 0.9f, 0.8f });
	leftHandParticleManager->SetLifeTimeRange(1.2f, 2.4f);
	leftHandParticleManager->SetSpeedRange(0.0f, 0.02f);
	leftHandParticleManager->SetUniformScaleRange(0.25f, 0.5f);
	leftHandParticleManager->SetScaleVelocityRange(0.1f, 0.2f);

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
	Vector3 skinningModelTranslate = object3d->GetTranslate();
	Vector3 skinningModelRotate = object3d->GetRotate();
	const float skinningModelMoveSpeed = 0.05f;
	const float skinningModelRotateSpeed = 0.03f;

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

		float currentMoveSpeed = skinningModelMoveSpeed;
	
		if (input->PushKey(DIK_W)) {
			skinningModelTranslate.z += currentMoveSpeed;
		} else if(input->PushKey(DIK_S)) {
			skinningModelTranslate.z -= currentMoveSpeed;
		}

		if (input->PushKey(DIK_A)) {
			skinningModelTranslate.x -= currentMoveSpeed;
		} else if (input->PushKey(DIK_D)) {
			skinningModelTranslate.x += currentMoveSpeed;
		}

		if (input->PushKey(DIK_LEFT)) {
			skinningModelRotate.y -= skinningModelRotateSpeed;
		} else if (input->PushKey(DIK_RIGHT)) {
			skinningModelRotate.y += skinningModelRotateSpeed;
		}

		object3d->SetTranslate(skinningModelTranslate);
		object3d->SetRotate(skinningModelRotate);

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

		srvManager->PreDraw();
		dxCommon->PreDraw();

		/*spriteCommon->CreatePrimitiveTopology();
		sprite->Update();
		sprite->Draw();*/

		camera->Update();
		keyframeAnimation->Update(1.0f / 60.0f);
		keyframeAnimation->ApplyAnimation(skeleton, keyframeAnimation->GetAnimation(), keyframeAnimation->GetAnimationTime());
		model->Update(skeleton);
		model->Update(skinCluster, skeleton);
		object3d->Update();
		object3d2->Update();

		if (rightHandJointIndex >= 0) {
			const Matrix4x4 rightHandWorldMatrix = Multiply(skeleton.joints[rightHandJointIndex].skeletonSpaceMatrix, object3d->GetWorldMatrix());
			const Vector3 rightHandPosition = {
				rightHandWorldMatrix.m[3][0],
				rightHandWorldMatrix.m[3][1] + 0.15f,
				rightHandWorldMatrix.m[3][2],
			};
			handParticleManager->Emit(rightHandPosition);
		}
		handParticleManager->Update(1.0f / 60.0f);
		if (leftHandJointIndex >= 0) {
			const Matrix4x4 leftHandWorldMatrix = Multiply(skeleton.joints[leftHandJointIndex].skeletonSpaceMatrix, object3d->GetWorldMatrix());
			const Vector3 leftHandPosition = {
				leftHandWorldMatrix.m[3][0],
				leftHandWorldMatrix.m[3][1] + 0.15f,
				leftHandWorldMatrix.m[3][2],
			};
			leftHandParticleManager->Emit(leftHandPosition);
		}
		leftHandParticleManager->Update(1.0f / 60.0f);

		/*skyBox->Update(camera);
		skyBox->CreatePrimitiveTopology();
		skyBox->Draw();*/

		/*ring->Update(camera);
		ring->CreatePrimitiveTopology();
		ring->Draw();*/

		/*cylinder->Update(camera);
		cylinder->CreatePrimitiveTopology();
		cylinder->Draw();*/

		object3dCommon->CreateSkinningPrimitiveTopology();
		object3d->Draw(skinCluster);

		object3dCommon->CreatePrimitiveTopology();
		object3d2->Draw();
		model->DrawSkeleton(skeleton, object3d->GetWorldMatrix(), camera);

		particleManager->Draw();
		particleManager2->Draw();
		flashManager->Draw();
		smokeManager->Draw();
		handParticleManager->Draw();
		leftHandParticleManager->Draw();
		

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
	delete handParticleManager;
	delete leftHandParticleManager;
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

