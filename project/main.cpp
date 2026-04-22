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

//struct TransformationMatrix {
//	Matrix4x4 WVP;
//	Matrix4x4 World;
//};

//struct DirectionalLight {
//	Vector4 color;
//	Vector3 direction;
//	float intensity;
//};

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
	//CoInitializeEx(0, COINIT_MULTITHREADED);
	//SetUnhandledExceptionFilter(ExportDump);

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
	Matrix4x4 worldMatrixSphere = MakeAffineMatrix(
		{ sphere.radius, sphere.radius, sphere.radius }, // スケール
		{ 0.0f, 0.0f, 0.0f },                           // 回転
		sphere.center                                   // 平行移動
	);
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
	ModelManager::GetInstance()->LoadModel("axis.obj"); //.objからモデルを読み込む
	modelCommon->Initialize(dxCommon);
	model->initialize(modelCommon, "resources", "axis.obj");
	terrainModel->initialize(modelCommon, "resources", "terrain.obj");
	object3dCommon->Initialize(dxCommon);
	camera->SetRotate({ 200.0f,0.0f,0.0f });
	//camera->SetTranslate({ 0.0f,0.0f,0.0f });
	object3d->Initialize(object3dCommon);
	object3d->SetModel(model);
	object3d->SetCamera(camera);
	object3d2->Initialize(object3dCommon);
	object3d2->SetModel(terrainModel);
	object3d2->SetCamera(camera);
	object3d2->SetTranslate({ 0.0f, -2.0f, 0.0f });
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

#endif

	

#ifdef _DEBUG

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

		cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
		viewMatrix = Inverse(cameraMatrix);

		YRotateSpeed += 0.01f;
		object3d->SetRotate({ 0.0f, YRotateSpeed, 0.0f });
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

		//ImGui
		//ImGui_ImplDX12_NewFrame();
		//ImGui_ImplWin32_NewFrame();
		//ImGui::NewFrame();
		//ImGui::ShowDemoWindow();
		//ImGui::DragFloat3("cameraScale", &cameraTransform.scale.x, 0.01f);
		//ImGui::DragFloat3("cameraRotate", &cameraTransform.rotate.x, 0.01f);
		//ImGui::DragFloat3("cameraTranslate", &cameraTransform.translate.x, 0.01f);
		//ImGui::DragFloat3("transform", &transform.translate.x, 0.01f);
		//ImGui::DragFloat2("transformSprite", &transformSprite.translate.x, 1.0f);
		//ImGui::DragFloat3("Light", &directionalLightData->direction.x, 0.01f);
		////ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 1.0f);
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
		object3dCommon->CreatePrimitiveTopology();
		camera->Update();
		object3d->Update();
		object3d2->Update();
		object3d->Draw();
		object3d2->Draw();

		

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
	delete srvManager;
	delete window;
	delete dxCommon;

	return 0;
}

//void Log(const std::string& message) {
//	OutputDebugStringA(message.c_str());
//}
//
//void Log(std::ostream& os, const std::string& message) {
//	os << message << std::endl;
//	OutputDebugStringA(message.c_str());
//}
//
//std::wstring ConvertString(const std::string& str) {
//	if (str.empty()) {
//		return std::wstring();
//	}
//
//	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
//	if (sizeNeeded == 0) {
//		return std::wstring();
//	}
//	std::wstring result(sizeNeeded, 0);
//	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
//	return result;
//}
//
//std::string ConvertString(const std::wstring& str) {
//	if (str.empty()) {
//		return std::string();
//	}
//
//	auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
//	if (sizeNeeded == 0) {
//		return std::string();
//	}
//	std::string result(sizeNeeded, 0);
//	WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
//	return result;
//}
//
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

//ID3D12DescriptorHeap* CreateDescriptorHeap(
//	ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
//
//	//ディスクリプタヒープの生成
//	ID3D12DescriptorHeap* descriptorHeap = nullptr;
//	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
//	descriptorHeapDesc.Type = heapType;
//	descriptorHeapDesc.NumDescriptors = numDescriptors;
//	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
//	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
//	assert(SUCCEEDED(hr));
//
//	return descriptorHeap;
//}
//
//DirectX::ScratchImage LoadTexture(const std::string& filePath) {
//	//テクスチャファイルを読んでプログラムで扱えるようにする
//	DirectX::ScratchImage image{};
//	std::wstring filePathW = ConvertString(filePath);
//
//	// デバッグ表示
//	std::wcout << L"Loading texture: " << filePathW << std::endl;
//
//	// ファイル存在確認
//	std::filesystem::path texturePath = std::filesystem::absolute(filePath);
//	std::wcout << L"Absolute path: " << texturePath.wstring() << std::endl;
//	if (!std::filesystem::exists(texturePath)) {
//		std::cerr << "File not found: " << texturePath << std::endl;
//		assert(false);
//	}
//	HRESULT hr = DirectX::LoadFromWICFile(texturePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
//
//	//if (!std::filesystem::exists(filePath)) {
//	//	std::cerr << "File not found: " << filePath << std::endl;
//	//	assert(false); // または return {};
//	//}
//	//HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
//
//	if (FAILED(hr)) {
//		std::wcout << L"Failed to load texture: " << filePathW << L", HRESULT = " << std::hex << hr << std::endl;
//	}
//
//	//assert(SUCCEEDED(hr));
//
//	//ミップマップの作成
//	DirectX::ScratchImage mipImages{};
//	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
//	assert(SUCCEEDED(hr));
//
//	return mipImages;
//}
//
//ID3D12Resource* CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata) {
//	//metadataを基にResourceの設定
//	D3D12_RESOURCE_DESC resourceDesc{};
//	resourceDesc.Width = UINT(metadata.width);
//	resourceDesc.Height = UINT(metadata.height);
//	resourceDesc.MipLevels = UINT16(metadata.mipLevels);
//	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
//	resourceDesc.Format = metadata.format;
//	resourceDesc.SampleDesc.Count = 1;
//	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);
//
//	//利用するHeapの設定
//	D3D12_HEAP_PROPERTIES heapProperties{};
//	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
//	//heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
//	//heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
//
//	//Resourceの生成
//	ID3D12Resource* resource = nullptr;
//	HRESULT hr = device->CreateCommittedResource(
//		&heapProperties,
//		D3D12_HEAP_FLAG_NONE,
//		&resourceDesc,
//		D3D12_RESOURCE_STATE_COPY_DEST,
//		nullptr,
//		IID_PPV_ARGS(&resource));
//	assert(SUCCEEDED(hr));
//
//	return resource;
//}
//
//[[nodiscard]]
//ID3D12Resource* UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages,
//	ID3D12Device* device, ID3D12GraphicsCommandList* commandList) {
//	//中間リソースの作成
//	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
//	DirectX::PrepareUpload(device, mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
//	uint64_t intermediateSize = GetRequiredIntermediateSize(texture, 0, UINT(subresources.size()));
//	ID3D12Resource* intermediateResource = CreateBufferResource(device, intermediateSize);
//	UpdateSubresources(commandList, texture, intermediateResource, 0, 0, UINT(subresources.size()), subresources.data());
//	D3D12_RESOURCE_BARRIER barrier{};
//	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
//	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
//	barrier.Transition.pResource = texture;
//	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
//	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
//	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
//	commandList->ResourceBarrier(1, &barrier);
//
//	return intermediateResource;
//
//	////Meta情報を取得
//	//const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
//
//	////	全MipMapについて
//	//for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {
//
//	//	const DirectX::Image* img = mipImages.GetImage(mipLevel, 0, 0);
//	//	HRESULT hr = texture->WriteToSubresource(
//	//		UINT(mipLevel),
//	//		nullptr,
//	//		img->pixels,
//	//		UINT(img->rowPitch),
//	//		UINT(img->slicePitch)
//	//	);
//	//	assert(SUCCEEDED(hr));
//	//}
//}
//
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


