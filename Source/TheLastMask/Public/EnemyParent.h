// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyParent.generated.h"

UCLASS()
class THELASTMASK_API AEnemyParent : public ACharacter
{
	GENERATED_BODY()

public: //Variables
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounds")
	FVector PatrolCenter;


protected:
	virtual void BeginPlay() override;

public:	//Methods
	AEnemyParent();
	//virtual void Tick(float DeltaTime) override;


};
