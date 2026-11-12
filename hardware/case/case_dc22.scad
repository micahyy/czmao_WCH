// ============================================================================
//  DC22 键盘外壳 —— 由 STEP(../pcb/3D_dc22_v1.0_260901_2026-11-06.step) 自动驱动
//  几何数据: ../pcb/board_data.scad / .json  (../tools/extract_step.py 生成)
//  【2026-11-12】DC22 全部资料已收拢到 DS22Lite/，本文件在 DS22Lite/case/ 下
//
//  【v4 / 2026-11-06 23:00】按口头纠正重做底壳：
//    1) PCB 下 3mm 是静音棉层，内底面 z=-3 必须完整，不再被电池仓切穿
//    2) 取消 8° 斜楔，外底面全平（需求.md 第 24 行的 8° 作废）
//    3) 电池仓改为「从背面往上挖」的 5mm 盲槽，顶留 1.5mm 隔板
//    4) 配重改为实心金属块，嵌进背面沉台、与底面齐平，兼作电池仓盖
//
//  层高（局部坐标，PCB 下表面 z=0）：
//    PCB            z =  0 .. 1.6
//    静音棉          z = -3 .. 0      (3mm，铺在完整内底面上)
//    内底面          z = -3           (完整平面 / 静音棉托面)
//    隔板            z = -4.5 .. -3   (1.5mm，隔开电池仓与静音棉)
//    电池仓盲槽      z = -9.5 .. -4.5 (60x40，仓口朝背面)
//    配重沉台        z = -9.5 .. -7   (2.5mm 深，配重块嵌平)
//    外底面          z = -9.5         (平)
//    定位板          z =  5.1 .. 6.6
//    上壳            z =  6.6 .. 13.6
//
//  结构：上壳 + 下壳 + 配重（外壳三件）；定位板按需求 3 另作 1.5mm 功能件
//  装电池：取下背面配重块 → 露出仓口 → 电池从背面塞入 → 盖回配重块拧 M2
//  仓位仍罩住 PCB 底面 BAT1 插座，线从 PCB 底面直接下垂入仓
//
//  【3D 打印零支撑的现实约束 —— 必须知道】
//    平底 + 背面盲槽 + 完整静音棉托面 = 上下两层封闭面夹一个空腔。
//    单件 FDM 无论正放还是倒放，都会出现一处大跨度水平桥接：
//      正放(背面贴平台) → 60x40 仓顶隔板悬空铺满；
//      倒放(内底面贴平台) → 98x97 内底面悬空铺满。
//    这是几何决定的，单件做不到零支撑。两条出路：
//      A) 接受局部支撑，或改用 CNC / SLA / 打印服务；
//      B) 用 part=9 / 10 拆件版：底板倒着打(槽口朝上)、下框正着打(环形墙)，
//         两件各自零支撑，叠合后由四角 M2 一起锁进上壳。
//    USB 为顶沿下开的 U 形敞口；沉头座为垂直圆孔 + 水平平底。
//
//  part: 0=装配预览 1=底壳 2=定位板 3=上壳 4=定位板DXF 5=上壳DXF
//        6=爆炸图 7=完整装配STL 8=配重 9=拆件底板 10=拆件下框
// ============================================================================
include <../pcb/board_data.scad>

$fn = 48;
eps = 0.01;

cnc = false;
part = 0;

// ---- 需求 3 固定参数 ----
pcb_plate_gap = 3.5;
plate_t       = 1.5;
plate_hole    = 14;
top_h         = 7;
top_hole      = 19.05;    // 轴距=19.05 逐孔会相切；上壳改用整块开口(见 top_cutters)
usb_w         = 10;
usb_z_low     = 1.0;

// ---- 需求 v4：静音棉 / 平底 / 背面盲槽电池仓 ----
cavity      = 3.0;                    // PCB 底 -> 内底面 = 静音棉厚，整层保留
base_t      = 6.5;                    // 内底面往下的底板总厚
partition   = 1.5;                    // 电池仓顶隔板（静音棉托面之下）
pocket_d    = base_t - partition;     // 电池仓盲槽深 5.0
battery_w   = 60;                     // 电池仓宽（按实物电池改）
battery_h   = 40;                     // 电池仓长（按实物电池改）
weight_t    = 2.5;                    // 配重块厚（实心金属，嵌平背面）
weight_lip  = 6;                      // 配重块搭接边（容纳 M2 固定孔）
fit_gap     = cnc ? 0.15 : 0.25;     // 配重块与沉台的装配间隙（零间隙实物装不进）

// ---- 需求 v3：M2 从下壳开孔 ----
screw_depth  = 4.5;      // 上壳攻丝盲孔深
head_r       = 2.5;      // M2 螺丝头 Ø5 沉孔
head_depth   = 1.6;      // 平底沉头座深
weight_pilot = 2.2;      // 配重块固定盲孔深

// ---- 工艺参数 ----
clearance   = cnc ? 0.3 : 0.6;
wall        = cnc ? 4.0 : 3.0;
corner_r    = cnc ? 1.5 : 0.4;
screw_pilot = cnc ? 0.8 : 1.0;    // 上壳：Ø1.6 攻丝底孔 / Ø2.0 自攻孔
screw_clear = cnc ? 1.1 : 1.25;   // 下壳+定位板：Ø2.2 / Ø2.5 过孔
ring_in     = cnc ? 4.0 : 3.0;
frame_extra = clearance + wall + 5.0;   // 四角要完整包住 Ø5 沉头座
frame_r     = 10;
bezel       = 6.0;                      // 上壳边框等宽；必须 < frame_r

// ---- 标高 ----
z_floor_top  = -cavity;                 // -3    内底面 / 静音棉托面
z_back       = z_floor_top - base_t;    // -9.5  外底面（平）
z_pocket_top = z_back + pocket_d;       // -4.5  电池仓槽顶
z_pcb_bot    = 0;
z_pcb_top    = board_t;
z_plate_bot  = z_pcb_top + pcb_plate_gap;
z_plate_top  = z_plate_bot + plate_t;
z_top_bot    = z_plate_top;
z_top_top    = z_plate_top + top_h;

x0 = board_cx - board_w/2;
x1 = board_cx + board_w/2;
y0 = board_cy - board_h/2;
y1 = board_cy + board_h/2;

// 四角螺丝：藏在规整外框的实心角里，不外凸
screw_off = 3.5;
screw_xy = [ [x0-screw_off, y0-screw_off], [x1+screw_off, y0-screw_off],
             [x1+screw_off, y1+screw_off], [x0-screw_off, y1+screw_off] ];

frame_w = board_w + 2*frame_extra;
frame_h = board_h + 2*frame_extra;

// PCB 底面电池插座 BAT1（STEP 实例坐标）：电池仓必须罩住它
bat_x = 20.0662;
bat_y = 86.3099;
battery_cx = bat_x + 22;      // 仓 x 12.07..72.07，BAT1 距左沿 8mm
battery_cy = bat_y - 16;      // 仓 y 50.31..90.31，BAT1 距上沿 4mm

// 上壳键区开口（坐标来自 STEP 键位；19.05 行距与孔相切 → 合并挖空）
key_u   = top_hole;
key_lx  = 9.0170;
key_rx0 = 32.8296 - key_u/2;
key_rx1 = 90.1702 + key_u/2;
key_y0  = 14.2613 - key_u/2;
key_y1  = 90.4615 + key_u/2;

bw = battery_w + 2*weight_lip;
bh = battery_h + 2*weight_lip;
// 配重块四角固定孔（绝对坐标，落在 6mm 搭接边中线上）
weight_screw = [
  [battery_cx - bw/2 + weight_lip/2, battery_cy - bh/2 + weight_lip/2],
  [battery_cx + bw/2 - weight_lip/2, battery_cy - bh/2 + weight_lip/2],
  [battery_cx + bw/2 - weight_lip/2, battery_cy + bh/2 - weight_lip/2],
  [battery_cx - bw/2 + weight_lip/2, battery_cy + bh/2 - weight_lip/2]
];

ZH = 60;
Z0 = -30;

// ============================ 2D 轮廓 ============================
module board_2d() { polygon(board_poly); }
module inner_2d() { offset(clearance) board_2d(); }

module rounded_rect(w, h, r) {
  rr = min(r, min(w,h)/2 - 0.01);
  hull()
    for (x = [-1,1], y = [-1,1])
      translate([x*(w/2-rr), y*(h/2-rr)]) circle(r = rr);
}

module frame_2d() {
  translate([board_cx, board_cy]) rounded_rect(frame_w, frame_h, frame_r);
}

module support_ring_2d() {
  difference() {
    offset(-1.2) board_2d();
    offset(-ring_in) board_2d();
  }
}

module rounded_square(s, r) {
  rr = min(r, s/2 - 0.001);
  hull()
    for (x = [-1,1], y = [-1,1])
      translate([x*(s/2-rr), y*(s/2-rr)]) circle(r = rr);
}

module switch_cutters(sz) {
  for (p = switch_xy)
    translate([p[0], p[1], Z0])
      linear_extrude(ZH) rounded_square(sz, corner_r);
}

module corner_holes(r, z_from, h) {
  for (p = screw_xy)
    translate([p[0], p[1], z_from]) cylinder(r = r, h = h, $fn = 24);
}

module top_opening_2d() {
  translate([board_cx, board_cy])
    rounded_rect(frame_w - 2*bezel, frame_h - 2*bezel, frame_r - bezel);
}
module top_cutters() {
  translate([0,0,Z0]) linear_extrude(ZH) top_opening_2d();
}
module top_cutters_2d() { top_opening_2d(); }

// ============================ v4 底壳要素 ============================
// 平底板：顶面 z=-3 是完整静音棉托面，底面 z=-9.5 全平
module base_plate() {
  translate([0,0,z_back]) linear_extrude(base_t) frame_2d();
}

// 电池仓：从背面(外底面)往上挖的盲槽，槽顶 z=-4.5，上方留 1.5mm 隔板
module battery_pocket() {
  translate([battery_cx - battery_w/2, battery_cy - battery_h/2, z_back - eps])
    cube([battery_w, battery_h, pocket_d + eps]);
}

// 配重沉台：比仓口大一圈的浅台阶，配重块嵌进去后底面与外壳齐平
module weight_recess() {
  translate([battery_cx - bw/2 - fit_gap, battery_cy - bh/2 - fit_gap, z_back - eps])
    cube([bw + 2*fit_gap, bh + 2*fit_gap, weight_t + eps]);
}

// 平底上的 M2 沉头座：螺丝头贴平，不凸出底面
module screw_seats() {
  for (p = screw_xy)
    translate([p[0], p[1], z_back - eps])
      cylinder(r = head_r, h = head_depth + eps, $fn = 24);
}

// 配重块固定盲孔：起于沉台底面 z=-7，往上 2.2mm 落入实体
module weight_pilots() {
  for (p = weight_screw)
    translate([p[0], p[1], z_back + weight_t - eps])
      cylinder(r = screw_pilot, h = weight_pilot + eps, $fn = 20);
}

module usb_cut() {
  translate([usb_x - usb_w/2, y1 - 2, usb_z_low])
    cube([usb_w, frame_extra + 6, z_plate_bot - usb_z_low + eps]);
}

// ============================ 零件 ============================
module bottom_shell() {
  difference() {
    union() {
      base_plate();
      // 外壁 + 四角实心螺丝柱（2D 预挖内腔，保住内腔里的托环）
      translate([0,0,z_floor_top])
        linear_extrude(z_plate_bot - z_floor_top)
          difference() { frame_2d(); inner_2d(); }
      // PCB 托环：顶在 z=0，高 3mm，静音棉铺在托环内侧
      translate([0,0,z_floor_top]) linear_extrude(cavity) support_ring_2d();
    }
    corner_holes(screw_clear, Z0, ZH);
    screw_seats();
    usb_cut();
    battery_pocket();
    weight_recess();
    weight_pilots();
  }
}

module switch_plate() {
  difference() {
    translate([0,0,z_plate_bot]) linear_extrude(plate_t) frame_2d();
    switch_cutters(plate_hole);
    corner_holes(screw_clear, Z0, ZH);
  }
}

module top_frame() {
  difference() {
    translate([0,0,z_top_bot]) linear_extrude(top_h) frame_2d();
    top_cutters();
    // M2 攻丝/自攻盲孔：螺丝从下壳上来拧进上壳，不穿透顶面
    corner_holes(screw_pilot, z_top_bot - eps, screw_depth + eps);
  }
}

// 实心配重块：72x52x2.5 金属板，嵌进背面沉台、底面齐平，兼作电池仓盖
module weight_plate() {
  difference() {
    translate([battery_cx - bw/2, battery_cy - bh/2, z_back])
      cube([bw, bh, weight_t]);
    for (p = weight_screw)
      translate([p[0], p[1], z_back - eps])
        cylinder(r = screw_clear, h = weight_t + 2*eps, $fn = 20);
  }
}

// ---- 零支撑拆件版（可选工艺 B）----
// part=9 底板：倒着打，内底面贴平台，电池仓槽口朝上，全程无桥接
module split_base() {
  difference() {
    base_plate();
    corner_holes(screw_clear, Z0, ZH);
    screw_seats();
    battery_pocket();
    weight_recess();
    weight_pilots();
  }
}

// 拆件下框：托环与侧壁之间隔着 clearance+1.2mm，整件时靠底板相连；
// 拆件后没有底板，补一圈 0.8mm 薄桥把两者焊成单一实体（内边界正好是静音棉外沿）
module frame_bridge() {
  translate([0,0,z_floor_top])
    linear_extrude(0.8)
      difference() { inner_2d(); offset(-ring_in) board_2d(); }
}

// part=10 下框：侧壁环 + 托环，正着打，环形墙垂直生长，无桥接
module split_frame() {
  difference() {
    union() {
      frame_bridge();
      translate([0,0,z_floor_top])
        linear_extrude(z_plate_bot - z_floor_top)
          difference() { frame_2d(); inner_2d(); }
      translate([0,0,z_floor_top]) linear_extrude(cavity) support_ring_2d();
    }
    corner_holes(screw_clear, Z0, ZH);
    usb_cut();
  }
}

module pcb_preview() {
  color([0.06,0.29,0.23])
    translate([0,0,z_pcb_bot]) linear_extrude(board_t) board_2d();
}

// 静音棉：PCB 下完整 3mm，铺在托环内侧，只作预览不进 STL
module foam_preview() {
  color([0.84,0.82,0.77])
    translate([0,0,z_floor_top]) linear_extrude(cavity) offset(-ring_in) board_2d();
}

// ============================ 2D 图纸 ============================
module plate_drawing() {
  difference() {
    frame_2d();
    for (p = switch_xy) translate(p) rounded_square(plate_hole, corner_r);
    for (p = screw_xy) translate(p) circle(r = screw_clear, $fn = 24);
  }
}

module top_drawing() {
  difference() {
    frame_2d();
    top_cutters_2d();
    for (p = screw_xy) translate(p) circle(r = screw_pilot, $fn = 24);
  }
}

// ============================ 输出 ============================
translate([-board_cx, -board_cy, 0]) {
  if (part == 0) {
    color([0.17,0.18,0.20]) bottom_shell();
    foam_preview();
    // 仅预览：抬高 0.12 避开与上壳底面共面，消除 OpenCSG 斜纹；STL 几何不变
    translate([0,0,0.12]) color([0.40,0.43,0.47]) switch_plate();
    color([0.10,0.11,0.13]) top_frame();
    color([0.22,0.34,0.48]) weight_plate();
    pcb_preview();
  } else if (part == 1) {
    bottom_shell();
  } else if (part == 2) {
    switch_plate();
  } else if (part == 3) {
    top_frame();
  } else if (part == 4) {
    plate_drawing();
  } else if (part == 5) {
    top_drawing();
  } else if (part == 6) {
    color([0.17,0.18,0.20]) bottom_shell();
    translate([0,0,-16]) color([0.22,0.34,0.48]) weight_plate();
    translate([0,0,10]) foam_preview();
    translate([0,0,20]) pcb_preview();
    translate([0,0,32]) color([0.40,0.43,0.47]) switch_plate();
    translate([0,0,44]) color([0.10,0.11,0.13]) top_frame();
  } else if (part == 7) {
    union() {
      bottom_shell();
      switch_plate();
      top_frame();
      weight_plate();
    }
  } else if (part == 8) {
    weight_plate();
  } else if (part == 9) {
    split_base();
  } else {
    split_frame();
  }
}
