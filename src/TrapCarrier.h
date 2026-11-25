// SPDX-License-Identifier: GPL-3.0-only
#pragma once
namespace trap {
// The condition-specific inventory forms carry all the weight.
// The zero-weight WEAP is the persistent hotkey, including while stock is empty.
constexpr char grantCarrier[]=R"(ref tool
ref stock
begin Function {tool stock}
 if eval !(player.GetItemCount tool) && (player.GetItemCount stock)
  player.AddItem tool 1 1
  SetFunctionValue 1
 endif
end)";

constexpr char bestStock[]=R"(ref stock
ref item
int i
begin Function {stock}
 let i := ListGetCount stock
 while i > 0
  let i -= 1
  let item := ListGetNthForm stock i
  if player.GetItemCount item
   SetFunctionValue item
   return
  endif
 loop
end)";

constexpr char placeCarrier[]=R"(ref tool
ref stock
ref item
ref cellRef
int fx
int fy
int fz
int count
int latched
int yaw16
float px
float py
float pz
float highZ
float lowZ
float hit
float yaw
begin Function {tool stock fx fy fz latched yaw16}
 if eval player.GetEquippedWeapon != tool || !player.IsWeaponOut
  return
 endif
 let count := player.GetItemCount stock
 if count < 1
  return
 endif
 let px := fx / 16.0
 let py := fy / 16.0
 let pz := fz / 16.0
 let highZ := pz + 10
 let lowZ := pz - 8
 let hit := GetP2PRayCastRange px py highZ px py lowZ 6
 if eval hit > 0 && lowZ + hit + 3 > pz
  let pz := lowZ + hit + 3
 endif
 let cellRef := player.GetParentCell
 let item := PlaceAtCell stock 1 cellRef px py pz
 if eval !item
  return
 endif
 if latched
  let yaw := yaw16 / 16.0
 else
  let yaw := player.GetAngle Z + 90
 endif
 item.SetAngle Z yaw
 item.SetPosEx px py pz
 player.RemoveItem stock 1 1
 if eval player.GetItemCount stock != (count - 1)
  item.Disable
  item.MarkForDelete
  return
 endif
 if latched
  item.AuxVarSetFlt "_TTWVRClutterPlaced" 2
 endif
 player.UnequipItem tool 0 1
 SetFunctionValue item
end)";
}
