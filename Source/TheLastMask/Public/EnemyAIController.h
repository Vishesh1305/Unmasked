#pragma once

#include "CoreMinimal.h"
#include "Runtime/AIModule/Classes/AIController.h"
#include "EnemyAIController.generated.h"

class UBehaviorTree;

UCLASS()
class THELASTMASK_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyAIController();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Default")
	TObjectPtr<UBehaviorTree> BehaviorTree;
protected:
	virtual void OnPossess(APawn* InPawn) override;
	
};
