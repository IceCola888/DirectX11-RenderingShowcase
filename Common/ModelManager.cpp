#include "XUtil.h"
#include "ModelManager.h"
#include "TextureManager.h"
#include "ImGuiLog.h"

#include <filesystem>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

using namespace DirectX;

void Model::CreateFromFile(Model& model, ID3D11Device* device, std::string_view filename)
{
    using namespace Assimp;
    namespace fs = std::filesystem;

    model.materials.clear();
    model.meshdatas.clear();
    model.boundingbox = BoundingBox();

    Importer importer;
    // 内部の点および線プリミティブを除去
    importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);
    auto pAssimpScene = importer.ReadFile(filename.data(),
        aiProcess_ConvertToLeftHanded |     // 左手系に変換
        aiProcess_GenBoundingBoxes |        // コリジョンボックスを取得
        aiProcess_Triangulate |             // ポリゴンを分割
        aiProcess_ImproveCacheLocality |    // キャッシュの局所性を改善
        aiProcess_SortByPType |             // プリミティブの頂点数でソートし、非三角形プリミティブを削除
        aiProcess_CalcTangentSpace);             

    if (pAssimpScene && !(pAssimpScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) && pAssimpScene->HasMeshes())
    {
        model.meshdatas.resize(pAssimpScene->mNumMeshes);
        model.materials.resize(pAssimpScene->mNumMaterials);
        for (uint32_t i = 0; i < pAssimpScene->mNumMeshes; ++i)
        {
            // 独自定義のメッシュとして保存
            // Assimp定義のメッシュデータ形式として保存
            auto& mesh = model.meshdatas[i];
            auto pAiMesh = pAssimpScene->mMeshes[i];

            // pAiMesh の頂点数を numVertices に代入
            // 後のコードでこの値を明確に使用できるようにする
            uint32_t numVertices = pAiMesh->mNumVertices;

            CD3D11_BUFFER_DESC bufferDesc(0, D3D11_BIND_VERTEX_BUFFER);
            D3D11_SUBRESOURCE_DATA initData{ nullptr, 0, 0 };
            // 位置
            if (pAiMesh->mNumVertices > 0)
            {
                // 初期化データの頂点データポインタを設定
                initData.pSysMem = pAiMesh->mVertices;
                // 頂点バッファのサイズ
                bufferDesc.ByteWidth = numVertices * sizeof(XMFLOAT3);
                device->CreateBuffer(&bufferDesc, &initData, mesh.m_pVertices.GetAddressOf());

                // 頂点配列内の全ての頂点を操作し、最小値 Vmin と最大値 Vmax を検索し、最初の項目に保存
                BoundingBox::CreateFromPoints(mesh.m_BoundingBox, numVertices,
                    (const XMFLOAT3*)pAiMesh->mVertices, sizeof(XMFLOAT3));
                // 最初のメッシュのバウンディングボックスが全体のバウンディングボックスとなる
                // 追加されるメッシュに応じて、バウンディングボックスも拡大する
                if (i == 0) {
                    model.boundingbox = mesh.m_BoundingBox;
                }
                else {
                    model.boundingbox.CreateMerged(model.boundingbox, model.boundingbox, mesh.m_BoundingBox);
                }

            }

            // 法線
            if (pAiMesh->HasNormals())// 法線が含まれているか確認
            {
                initData.pSysMem = pAiMesh->mNormals;
                bufferDesc.ByteWidth = numVertices * sizeof(XMFLOAT3);
                bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                device->CreateBuffer(&bufferDesc, &initData, mesh.m_pNormals.GetAddressOf());
            }

            // 接線および副接線
            if (pAiMesh->HasTangentsAndBitangents())
            {
                // w成分は手性の保存やGPU操作の利便性のために使用される可能性がある
                std::vector<XMFLOAT4> tangents(numVertices, XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
                for (uint32_t i = 0; i < pAiMesh->mNumVertices; ++i)
                {
                    // データ転送
                    memcpy_s(&tangents[i], sizeof(XMFLOAT3),
                        pAiMesh->mTangents + i, sizeof(XMFLOAT3));
                }

                initData.pSysMem = tangents.data();
                bufferDesc.ByteWidth = pAiMesh->mNumVertices * sizeof(XMFLOAT4);
                device->CreateBuffer(&bufferDesc, &initData, mesh.m_pTangents.GetAddressOf());

                for (uint32_t i = 0; i < pAiMesh->mNumVertices; ++i)
                {
                    memcpy_s(&tangents[i], sizeof(XMFLOAT3),
                        pAiMesh->mBitangents + i, sizeof(XMFLOAT3));
                }
                device->CreateBuffer(&bufferDesc, &initData, mesh.m_pBitangents.GetAddressOf());
            }

            // テクスチャ座標
            // Assimpは各頂点に最大8セットのテクスチャ座標をサポート
            uint32_t numUVs = 8;
            while (numUVs && !pAiMesh->HasTextureCoords(numUVs - 1))
                numUVs--;

            if (numUVs > 0)
            {
                // テクスチャ座標セットを走査 / すべての頂点を走査
                mesh.m_pTexcoordArrays.resize(numUVs);
                for (uint32_t i = 0; i < numUVs; ++i)
                {
                    std::vector<XMFLOAT2> uvs(numVertices);
                    for (uint32_t j = 0; j < numVertices; ++j)
                    {
                        // 各頂点の各テクスチャ座標をコピー
                        memcpy_s(&uvs[j], sizeof(XMFLOAT2),
                            pAiMesh->mTextureCoords[i] + j, sizeof(XMFLOAT2));
                    }
                    initData.pSysMem = uvs.data();
                    bufferDesc.ByteWidth = numVertices * sizeof(XMFLOAT2);
                    device->CreateBuffer(&bufferDesc, &initData, mesh.m_pTexcoordArrays[i].GetAddressOf());
                }
            }

            // インデックス
            uint32_t numFaces = pAiMesh->mNumFaces;
            uint32_t numIndices = numFaces * 3;
            if (numFaces > 0)
            {
                mesh.m_IndexCount = numIndices;
                if (numIndices < 65535)
                {
                    std::vector<uint16_t> indices(numIndices);
                    for (size_t i = 0; i < numFaces; ++i)
                    {
                        indices[i * 3] = static_cast<uint16_t>(pAiMesh->mFaces[i].mIndices[0]);
                        indices[i * 3 + 1] = static_cast<uint16_t>(pAiMesh->mFaces[i].mIndices[1]);
                        indices[i * 3 + 2] = static_cast<uint16_t>(pAiMesh->mFaces[i].mIndices[2]);
                    }
                    bufferDesc = CD3D11_BUFFER_DESC(numIndices * sizeof(uint16_t), D3D11_BIND_INDEX_BUFFER);
                    initData.pSysMem = indices.data();
                    device->CreateBuffer(&bufferDesc, &initData, mesh.m_pIndices.GetAddressOf());
                }
                else
                {
                    std::vector<uint32_t> indices(numIndices);
                    for (size_t i = 0; i < numFaces; ++i)
                    {
                        memcpy_s(indices.data() + i * 3, sizeof(uint32_t) * 3,
                            pAiMesh->mFaces[i].mIndices, sizeof(uint32_t) * 3);
                    }
                    bufferDesc = CD3D11_BUFFER_DESC(numIndices * sizeof(uint32_t), D3D11_BIND_INDEX_BUFFER);
                    initData.pSysMem = indices.data();
                    device->CreateBuffer(&bufferDesc, &initData, mesh.m_pIndices.GetAddressOf());
                }
            }
            // マテリアルインデックス
            mesh.m_MaterialIndex = pAiMesh->mMaterialIndex;
        }






        for (uint32_t i = 0; i < pAssimpScene->mNumMaterials; ++i)
        {
            auto& material = model.materials[i];

            auto pAiMaterial = pAssimpScene->mMaterials[i];
            XMFLOAT4 vec{};
            float value{};
            uint32_t boolean{};
            uint32_t num = 3;

            if (aiReturn_SUCCESS == pAiMaterial->Get(AI_MATKEY_COLOR_AMBIENT, (float*)&vec, &num))
                material.Set("$AmbientColor", vec);
            if (aiReturn_SUCCESS == pAiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, (float*)&vec, &num))
                material.Set("$DiffuseColor", vec);
            if (aiReturn_SUCCESS == pAiMaterial->Get(AI_MATKEY_COLOR_SPECULAR, (float*)&vec, &num))
                material.Set("$SpecularColor", vec);
            if (aiReturn_SUCCESS == pAiMaterial->Get(AI_MATKEY_SPECULAR_FACTOR, value))
                material.Set("$SpecularFactor", value);
            if (aiReturn_SUCCESS == pAiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, (float*)&vec, &num))
                material.Set("$EmissiveColor", vec);
            if (aiReturn_SUCCESS == pAiMaterial->Get(AI_MATKEY_OPACITY, value))
                material.Set("$Opacity", value);
            if (aiReturn_SUCCESS == pAiMaterial->Get(AI_MATKEY_COLOR_TRANSPARENT, (float*)&vec, &num))
                material.Set("$TransparentColor", vec);
            if (aiReturn_SUCCESS == pAiMaterial->Get(AI_MATKEY_COLOR_REFLECTIVE, (float*)&vec, &num))
                material.Set("$ReflectiveColor", vec);

            aiString aiPath;
            fs::path texFilename;
            std::string texName;

            // [&] はすべての外部変数を参照キャプチャすることを示す
            auto TryCreateTexture = [&](aiTextureType type, std::string_view propertyName, uint32_t loadConfig) {
                if (!pAiMaterial->GetTextureCount(type))
                    return;

                pAiMaterial->GetTexture(type, 0, &aiPath);

                // テクスチャは事前にロードされている
                if (aiPath.data[0] == '*')
                {
                    texName = filename;
                    texName += aiPath.C_Str();
                    char* pEndStr = nullptr;
                    aiTexture* pTex = pAssimpScene->mTextures[strtol(aiPath.data + 1, &pEndStr, 10)];
                    TextureManager::Get().CreateFromMemory(texName, pTex->pcData, pTex->mHeight ? pTex->mWidth * pTex->mHeight : pTex->mWidth,
                        loadConfig);
                    material.Set(propertyName, std::string(texName));
                }
                // テクスチャはファイル名でインデックス化される
                else
                {
                    texFilename = filename;
                    texFilename = texFilename.parent_path() / aiPath.C_Str();
                    TextureManager::Get().CreateFromFile(texFilename.string(), loadConfig);
                    material.Set(propertyName, texFilename.string());
                }
            };

            TryCreateTexture(aiTextureType_DIFFUSE, "$Diffuse", TextureManager::LoadConfig_EnableMips | TextureManager::LoadConfig_ForceSRGB);
            TryCreateTexture(aiTextureType_NORMALS, "$Normal", 0);
            TryCreateTexture(aiTextureType_BASE_COLOR, "$Albedo", TextureManager::LoadConfig_EnableMips | TextureManager::LoadConfig_ForceSRGB);
            TryCreateTexture(aiTextureType_METALNESS, "$Metallic", 0);
            TryCreateTexture(aiTextureType_DIFFUSE_ROUGHNESS, "$Roughness", 0);
            TryCreateTexture(aiTextureType_AMBIENT_OCCLUSION, "$AmbientOcclusion", 0);
            /*TryCreateTexture(aiTextureType_DISPLACEMENT, "$Displacement", 0);
            TryCreateTexture(aiTextureType_HEIGHT, "$Height", 0);*/
        }
    }
    else
    {
        std::string warning = "[Warning]: ModelManager::CreateFromFile, failed to load \"";
        warning += filename;
        warning += "\"\n";

        if (ImGuiLog::HasInstance())
        {
            ImGuiLog::Get().AddLog(warning.c_str());
        }
        else
        {
            OutputDebugStringA(warning.c_str());
        }
    }
}

void Model::CreateFromGeometry(Model& model, ID3D11Device* device, const GeometryData& data, bool isDynamic)
{
    // デフォルトマテリアル
    model.materials = { Material{} };
    model.materials[0].Set("$Albedo", XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f));
    model.materials[0].Set("$AmbientColor", XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f));
    model.materials[0].Set("$DiffuseColor", XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f));
    model.materials[0].Set("$SpecularColor", XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f));
    model.materials[0].Set("$SpecularFactor", 10.0f);
    model.materials[0].Set("$Roughness", 0.5f);
    model.materials[0].Set("$Metallic", 0.0f);
    model.materials[0].Set("$Opacity", 1.0f);

    model.meshdatas = { MeshData{} };
    model.meshdatas[0].m_pTexcoordArrays.resize(1);
    model.meshdatas[0].m_VertexCount = (uint32_t)data.vertices.size();
    model.meshdatas[0].m_IndexCount = (uint32_t)(!data.indices16.empty() ? data.indices16.size() : data.indices32.size());
    model.meshdatas[0].m_MaterialIndex = 0;

    CD3D11_BUFFER_DESC bufferDesc(0,
        D3D11_BIND_VERTEX_BUFFER,
        isDynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT,
        isDynamic ? D3D11_CPU_ACCESS_WRITE : 0);
    D3D11_SUBRESOURCE_DATA initData{ nullptr, 0, 0 };

    initData.pSysMem = data.vertices.data();
    bufferDesc.ByteWidth = (uint32_t)data.vertices.size() * sizeof(XMFLOAT3);
    device->CreateBuffer(&bufferDesc, &initData, model.meshdatas[0].m_pVertices.GetAddressOf());

    if (!data.normals.empty())
    {
        initData.pSysMem = data.normals.data();
        bufferDesc.ByteWidth = (uint32_t)data.normals.size() * sizeof(XMFLOAT3);
        device->CreateBuffer(&bufferDesc, &initData, model.meshdatas[0].m_pNormals.GetAddressOf());
    }

    if (!data.texcoords.empty())
    {
        initData.pSysMem = data.texcoords.data();
        bufferDesc.ByteWidth = (uint32_t)data.texcoords.size() * sizeof(XMFLOAT2);
        device->CreateBuffer(&bufferDesc, &initData, model.meshdatas[0].m_pTexcoordArrays[0].GetAddressOf());
    }

    if (!data.tangents.empty())
    {
        initData.pSysMem = data.tangents.data();
        bufferDesc.ByteWidth = (uint32_t)data.tangents.size() * sizeof(XMFLOAT4);
        device->CreateBuffer(&bufferDesc, &initData, model.meshdatas[0].m_pTangents.GetAddressOf());
    }

    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.CPUAccessFlags = 0;
    if (!data.indices16.empty())
    {
        initData.pSysMem = data.indices16.data();
        bufferDesc = CD3D11_BUFFER_DESC((uint16_t)data.indices16.size() * sizeof(uint16_t), D3D11_BIND_INDEX_BUFFER);
        device->CreateBuffer(&bufferDesc, &initData, model.meshdatas[0].m_pIndices.GetAddressOf());
    }
    else
    {
        initData.pSysMem = data.indices32.data();
        bufferDesc = CD3D11_BUFFER_DESC((uint32_t)data.indices32.size() * sizeof(uint32_t), D3D11_BIND_INDEX_BUFFER);
        device->CreateBuffer(&bufferDesc, &initData, model.meshdatas[0].m_pIndices.GetAddressOf());
    }
}

void Model::SetDebugObjectName(std::string_view name)
{
#if (defined(DEBUG) || defined(_DEBUG)) && (GRAPHICS_DEBUGGER_OBJECT_NAME)
    std::string baseStr = name.data();
    size_t sz = meshdatas.size();
    std::string str;
    str.reserve(100);
    for (size_t i = 0; i < sz; ++i)
    {
        baseStr = name.data();
        baseStr += "[" + std::to_string(i) + "].";
        if (meshdatas[i].m_pVertices)
            ::SetDebugObjectName(meshdatas[i].m_pVertices.Get(), baseStr + "vertices");
        if (meshdatas[i].m_pNormals)
            ::SetDebugObjectName(meshdatas[i].m_pNormals.Get(), baseStr + "normals");
        if (meshdatas[i].m_pTangents)
            ::SetDebugObjectName(meshdatas[i].m_pTangents.Get(), baseStr + "tangents");
        if (meshdatas[i].m_pBitangents)
            ::SetDebugObjectName(meshdatas[i].m_pBitangents.Get(), baseStr + "bitangents");
        if (meshdatas[i].m_pColors)
            ::SetDebugObjectName(meshdatas[i].m_pColors.Get(), baseStr + "colors");
        if (!meshdatas[i].m_pTexcoordArrays.empty())
        {
            size_t texSz = meshdatas[i].m_pTexcoordArrays.size();
            for (size_t j = 0; j < texSz; ++j)
                ::SetDebugObjectName(meshdatas[i].m_pTexcoordArrays[j].Get(), baseStr + "uv" + std::to_string(j));
        }
        if (meshdatas[i].m_pIndices)
            ::SetDebugObjectName(meshdatas[i].m_pIndices.Get(), baseStr + "indices");
        if (meshdatas[i].m_pBBVB)
            ::SetDebugObjectName(meshdatas[i].m_pBBVB.Get(), baseStr + "BBvertices");
        if (meshdatas[i].m_pBBColors)
            ::SetDebugObjectName(meshdatas[i].m_pBBColors.Get(), baseStr + "BBcolors");
        if (meshdatas[i].m_pBBIB)
            ::SetDebugObjectName(meshdatas[i].m_pBBIB.Get(), baseStr + "BBindices");
    }
#else
    UNREFERENCED_PARAMETER(name);
#endif
}

namespace
{
    // ModelManagerシングルトン
    // プログラムの起動時（具体的にはmain関数の実行前）に初期化される
    ModelManager* s_pInstance = nullptr;
}


ModelManager::ModelManager()
{
    // 既に作成されている場合は例外をスローする
    if (s_pInstance)
        throw std::exception("ModelManager is a singleton!");
    // その役割は、s_pInstance 静的ポインタを新しく作成された ModelManager オブジェクトに設定すること
    // これにより、s_pInstance は現在の（そして唯一の）ModelManager インスタンスのアドレスを保持する
    // 以降、このポインタを通じてそのインスタンスにアクセスできるようになる。
    s_pInstance = this;
}

ModelManager::~ModelManager()
{
}

ModelManager& ModelManager::Get()
{
    if (!s_pInstance)
        throw std::exception("ModelManager needs an instance!");
    return *s_pInstance;
}

void ModelManager::Init(ID3D11Device* device)
{
    m_pDevice = device;
    m_pDevice->GetImmediateContext(m_pDeviceContext.ReleaseAndGetAddressOf());
}

Model* ModelManager::CreateFromFile(std::string_view filename)
{
    return CreateFromFile(filename, filename);
}

Model* ModelManager::CreateFromFile(std::string_view name, std::string_view filename)
{
    XID modelID = StringToID(name);
    auto& model = m_Models[modelID];
    Model::CreateFromFile(model, m_pDevice.Get(), filename);
    return &model;
}

Model* ModelManager::CreateFromGeometry(std::string_view name, const GeometryData& data, bool isDynamic)
{
    XID modelID = StringToID(name);
    auto& model = m_Models[modelID];
    Model::CreateFromGeometry(model, m_pDevice.Get(), data, isDynamic);

    return &model;
}

const Model* ModelManager::GetModel(std::string_view name) const
{
    XID nameID = StringToID(name);
    if (auto it = m_Models.find(nameID); it != m_Models.end())
        return &it->second;
    return nullptr;
}

Model* ModelManager::GetModel(std::string_view name)
{
    XID nameID = StringToID(name);
    if (m_Models.count(nameID))
        return &m_Models[nameID];
    return nullptr;
}
