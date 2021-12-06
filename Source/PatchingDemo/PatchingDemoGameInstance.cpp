// Fill out your copyright notice in the Description page of Project Settings.


#include "PatchingDemoGameInstance.h"

#include "Misc/CoreDelegates.h"
#include "AssetRegistryModule.h"
#include "ChunkDownloader.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Runtime/Engine/Public/EngineGlobals.h"

void UPatchingDemoGameInstance::OnManifestUpdateComplete(bool bSuccess)
{
    bIsDownloadManifestUpToDate = bSuccess;
}

void UPatchingDemoGameInstance::Init()
{
    Super::Init();

    const FString DeploymentName = "PakLoadingDemoLive";
    const FString ContentBuildId = "PakLoadingDemoKey";

    // initialize the chunk downloader
    TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetOrCreate();
    Downloader->Initialize("Windows", 8);

    // load the cached build ID
    Downloader->LoadCachedBuild(DeploymentName);

    // update the build manifest file
    TFunction<void(bool bSuccess)> UpdateCompleteCallback = [&](bool bSuccess) {bIsDownloadManifestUpToDate = bSuccess; GEngine->AddOnScreenDebugMessage(-1, 12.f, FColor::White, TEXT("Download manifest up to date.")); };
    Downloader->UpdateBuild(DeploymentName, ContentBuildId, UpdateCompleteCallback);

    OnPatchComplete.AddDynamic(this, &UPatchingDemoGameInstance::PatchCompleteCallback);
}

void UPatchingDemoGameInstance::Shutdown()
{
    Super::Shutdown();

    // Shut down ChunkDownloader
    FChunkDownloader::Shutdown();
}

void UPatchingDemoGameInstance::GetLoadingProgress(int64& BytesDownloaded, int64& TotalBytesToDownload, float& DownloadPercent, int32& ChunksMounted, int32& TotalChunksToMount, float& MountPercent) const
{
    //Get a reference to ChunkDownloader
    TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

    //Get the loading stats struct
    FChunkDownloader::FStats LoadingStats = Downloader->GetLoadingStats();

    //Get the bytes downloaded and bytes to download
    BytesDownloaded = LoadingStats.BytesDownloaded;
    TotalBytesToDownload = LoadingStats.TotalBytesToDownload;

    //Get the number of chunks mounted and chunks to download
    ChunksMounted = LoadingStats.ChunksMounted;
    TotalChunksToMount = LoadingStats.TotalChunksToMount;

	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, FString::Printf(TEXT("Total Bytes To Download: %lld"), TotalBytesToDownload));
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, FString::Printf(TEXT("Bytes Downloaded: %lld"), BytesDownloaded));
    GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, FString::Printf(TEXT("Total Chunks To Mount: %d"), TotalChunksToMount));
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, FString::Printf(TEXT("ChunksMounted: %d"), ChunksMounted));

    //Calculate the download and mount percent using the above stats
    DownloadPercent = (float) BytesDownloaded / (float) TotalBytesToDownload;
    MountPercent = (TotalChunksToMount == 0) ? 0 : ChunksMounted / TotalChunksToMount;
}

void UPatchingDemoGameInstance::OnLoadingModeComplete(bool bSuccess)
{
    OnDownloadComplete(bSuccess);
}

void UPatchingDemoGameInstance::OnMountComplete(bool bSuccess)
{
    if (bSuccess)
    {
        GEngine->AddOnScreenDebugMessage(-1, 12.f, FColor::Green, TEXT("Mount complete"));
    }

    OnPatchComplete.Broadcast(bSuccess);
}

bool UPatchingDemoGameInstance::DownloadChunks()
{
    GEngine->AddOnScreenDebugMessage(-1, 12.f, FColor::Cyan, TEXT("DOWNLOADING CHUNKS"));

    // make sure the download manifest is up to date
    if (bIsDownloadManifestUpToDate)
    {
        // get the chunk downloader
        TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

        // report current chunk status
        for (int32 ChunkID : ChunkDownloadList)
        {
            GEngine->AddOnScreenDebugMessage(-1, 12.f, FColor::Cyan, FString::Printf(TEXT("%d"), ChunkID));
            int32 ChunkStatus = static_cast<int32>(Downloader->GetChunkStatus(ChunkID));
            UE_LOG(LogTemp, Display, TEXT("Chunk %i status: %i"), ChunkID, ChunkStatus);
        }

        GEngine->AddOnScreenDebugMessage(-1, 12.f, FColor::Cyan, FString::Printf(TEXT("Total Chunks To Download: %d"), ChunkDownloadList.Num()));

        TFunction<void(bool bSuccess)> DownloadCompleteCallback = [&](bool bSuccess) {OnDownloadComplete(bSuccess); };
        Downloader->DownloadChunks(ChunkDownloadList, DownloadCompleteCallback, 1);

        // start loading mode
        TFunction<void(bool bSuccess)> LoadingModeCompleteCallback = [&](bool bSuccess) {OnLoadingModeComplete(bSuccess); };
        Downloader->BeginLoadingMode(LoadingModeCompleteCallback);
        return true;
    }

    // we couldn't contact the server to validate our manifest, so we can't patch
    UE_LOG(LogTemp, Display, TEXT("Manifest Update Failed. Can't patch the game"));

    return false;

}

void UPatchingDemoGameInstance::OnDownloadComplete(bool bSuccess)
{
    if (bSuccess)
    {
        UE_LOG(LogTemp, Display, TEXT("Download complete"));
        GEngine->AddOnScreenDebugMessage(-1, 12.f, FColor::Green, TEXT("Download complete"));

        // get the chunk downloader
        TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

        FJsonSerializableArrayInt DownloadedChunks;

        for (int32 ChunkID : ChunkDownloadList)
        {
            DownloadedChunks.Add(ChunkID);
        }

        //Mount the chunks
        TFunction<void(bool bSuccess)> MountCompleteCallback = [&](bool bSuccess) {OnMountComplete(bSuccess); };
        Downloader->MountChunks(DownloadedChunks, MountCompleteCallback);

        OnPatchComplete.Broadcast(true);

    }
    else
    {

        UE_LOG(LogTemp, Display, TEXT("Load process failed"));

        // call the delegate
        OnPatchComplete.Broadcast(false);
    }
}

void UPatchingDemoGameInstance::PrintAssetRegistryMaps()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> AssetData;
	const UClass* Class = UWorld::StaticClass();
	AssetRegistryModule.Get().GetAssetsByClass(Class->GetFName(), AssetData);

	for (FAssetData Data : AssetData)
	{
		GEngine->AddOnScreenDebugMessage(-1, 12.f, FColor::White, Data.AssetName.ToString());
	}
}

void UPatchingDemoGameInstance::PatchCompleteCallback(bool bSuccess)
{
    bIsPatchComplete = true;

    if (bSuccess)
    {
        GEngine->AddOnScreenDebugMessage(-1, 12.f, FColor::Green, TEXT("Patch complete"));
    }

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> MapData;
	const UClass* WorldClass = UWorld::StaticClass();
	AssetRegistry.GetAssetsByClass(WorldClass->GetFName(), MapData);

	for (FAssetData Data : MapData)
	{
		UE_LOG(LogTemp, Log, TEXT("%s"), *(Data.AssetName.ToString()));
		GEngine->AddOnScreenDebugMessage(-1, 12.f, FColor::White, Data.AssetName.ToString());
	}
}