#pragma once

#include "AbilitySystemInterface.h"
#include "Abilities/VelrathAbilitySet.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VelrathCharacter.generated.h"

class UAbilitySystemComponent;
class UCameraComponent;
class UInputAction;
class UInputComponent;
class UInputMappingContext;
class USpringArmComponent;
class UVelrathAttributeSet;
class UVelrathCharacterMovementComponent;
struct FInputActionValue;

UCLASS()
class VELRATH_API AVelrathCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AVelrathCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void PawnClientRestart() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void UnPossessed() override;
    virtual void OnRep_PlayerState() override;
    virtual void Destroyed() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    const UVelrathAttributeSet* GetVelrathAttributeSet() const { return AttributeSet; }
    const UVelrathAbilitySet* GetStartupAbilitySet() const { return StartupAbilitySet; }
    bool AreDefaultAttributesInitialized() const { return bDefaultAttributesInitialized; }
    bool AreStartupAbilitiesGranted() const;
    int32 GetStartupAbilityHandleCount() const { return StartupAbilityHandles.Num(); }
    const TArray<FGameplayAbilitySpecHandle>& GetStartupAbilityHandles() const
    {
        return StartupAbilityHandles.GetAbilitySpecHandles();
    }
    bool GrantStartupAbilities();
    bool RemoveStartupAbilities();

#if WITH_EDITOR
    void SetStartupAbilitySet(UVelrathAbilitySet* InAbilitySet) { StartupAbilitySet = InAbilitySet; }
#endif

    USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
    UCameraComponent* GetFollowCamera() const { return FollowCamera; }
    float GetInitialCameraPitch() const { return InitialCameraPitch; }

    float GetWalkSpeed() const { return WalkSpeed; }
    float GetSprintSpeed() const { return SprintSpeed; }
    float GetBackwardMovementMultiplier() const { return BackwardMovementMultiplier; }
    float GetAttackMovementMultiplier() const { return AttackMovementMultiplier; }
    float GetBlockMovementMultiplier() const { return BlockMovementMultiplier; }
    UVelrathCharacterMovementComponent* GetVelrathCharacterMovement() const;

    const UInputMappingContext* GetDefaultMappingContext() const { return DefaultMappingContext; }
    const UInputAction* GetMoveAction() const { return MoveAction; }
    const UInputAction* GetLookAction() const { return LookAction; }
    const UInputAction* GetSprintAction() const { return SprintAction; }

private:
    void ApplyDefaultMappingContext();
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void StartSprinting();
    void StopSprinting();
    void InitializeAbilityActorInfo();
    void InitializeDefaultAttributes();
    void RetireAbilitySystem();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UVelrathAttributeSet> AttributeSet;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities|Startup", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UVelrathAbilitySet> StartupAbilitySet;

    UPROPERTY(Transient)
    bool bDefaultAttributesInitialized = false;

    UPROPERTY(Transient)
    bool bAuthoritativeAbilitySystemInitialized = false;

    UPROPERTY(Transient)
    bool bAbilitySystemRetired = false;

    UPROPERTY(Transient)
    FVelrathAbilitySetGrantedHandles StartupAbilityHandles;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UInputAction> SprintAction;

    // Starting framing hypothesis. Applied to the local PlayerController's ControlRotation after possession.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Configuration", meta = (AllowPrivateAccess = "true", ClampMin = "-89.0", ClampMax = "89.0", Units = "deg"))
    float InitialCameraPitch = -20.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Configuration", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm/s"))
    float WalkSpeed = 520.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Configuration", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm/s"))
    float SprintSpeed = 660.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Configuration", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float BackwardMovementMultiplier = 0.85f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Configuration", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float AttackMovementMultiplier = 0.45f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Configuration", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float BlockMovementMultiplier = 0.60f;
};

