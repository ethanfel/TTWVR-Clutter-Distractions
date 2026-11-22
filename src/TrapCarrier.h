// SPDX-License-Identifier: GPL-3.0-only
#pragma once
namespace trap {
// Inventory forms 800-804 remain the saved stock and carry all the weight.
// The zero-weight WEAP is the persistent hotkey, including while stock is empty.
constexpr char grantCarrier[]=R"(ref tool
ref c0
ref c1
ref c2
ref c3
ref c4
begin Function {tool c0 c1 c2 c3 c4}
 if eval !(player.GetItemCount tool) && ((player.GetItemCount c4) || (player.GetItemCount c3) || (player.GetItemCount c2) || (player.GetItemCount c1) || (player.GetItemCount c0))
  player.AddItem tool 1 1
  SetFunctionValue 1
 endif
end)";

constexpr char placeCarrier[]=R"(ref tool
ref stock
ref item
ref cellRef
int fx
int fy
int fz
int count
float px
float py
float highZ
float lowZ
float hit
float yaw
begin Function {tool stock fx fy fz}
 if eval player.GetEquippedWeapon != tool || !player.IsWeaponOut
  return
 endif
 let count := player.GetItemCount stock
 if count < 1
  return
 endif
 let px := fx / 16.0
 let py := fy / 16.0
 let highZ := fz / 16.0 + 8
 let lowZ := highZ - 54
 let hit := GetP2PRayCastRange px py highZ px py lowZ 6
 if hit <= 0
  MessageEx "Lower the trap closer to a surface."
  return
 endif
 let cellRef := player.GetParentCell
 let lowZ += hit + 3
 let item := PlaceAtCell stock 1 cellRef px py lowZ
 if eval !item
  return
 endif
 let yaw := player.GetAngle Z + 90
 item.SetAngle Z yaw
 player.RemoveItem stock 1 1
 if eval player.GetItemCount stock != (count - 1)
  item.Disable
  item.MarkForDelete
  return
 endif
 player.UnequipItem tool 0 1
 SetFunctionValue item
end)";
}
