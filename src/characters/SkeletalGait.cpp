#include "outland/assets/RaylibContract.hpp"
#include "outland/characters/SkeletalGait.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <functional>
namespace outland::characters {
namespace {
bool named(const BoneInfo& bone,const char* suffix) {
    const std::string_view name(bone.name,static_cast<std::size_t>(std::find(bone.name,bone.name+32,'\0')-bone.name));
    return name==suffix || name.ends_with(std::string(":")+suffix);
}
}
bool SkeletalGait::sample(const Model& model,AnimationAction action,double cycle,float ground_speed,float facing,
                          const BodyPose& body,float time,float metres) {
    holding_=false;
    if(!model.skeleton.bones || !model.skeleton.bindPose || model.skeleton.boneCount<=0 || model.skeleton.boneCount>256 ||
       !std::isfinite(cycle) || !std::isfinite(facing) || !std::isfinite(time)) return false;
    if(action==AnimationAction::None) return false;
    const bool dying=action==AnimationAction::Death;
    const bool attacking=action==AnimationAction::Attack;
    if(dying || attacking) action=AnimationAction::Idle;
    const bool zombie=body.style==BodyStyle::Zombie;
    auto find=[&](const char* name) {for(int i=0;i<model.skeleton.boneCount;++i) if(named(model.skeleton.bones[i],name))return i;return -1;};
    const int hips=find("Hips"),left=find("LeftUpLeg"),right=find("RightUpLeg");
    const int left_knee=find("LeftLeg"),right_knee=find("RightLeg");
    if(hips<0 || left<0 || right<0 || left_knee<0 || right_knee<0) return false;
    // Basis comes from the bind skeleton itself (hips->head is up, left->right hip is the swing
    // axis). The model's display transform is not the bone space: Z-up PSX bodies have Y-up bind
    // poses after import, and swinging about the display axes stretched arms across the screen.
    const auto* bind=model.skeleton.bindPose;
    int top=find("Head");
    if(top<0) top=find("Neck");
    // Without a head, the thigh (hip above knee) gives the vertical.
    const auto up=top>=0 ? Vector3Normalize(Vector3Subtract(bind[top].translation,bind[hips].translation)) :
        Vector3Normalize(Vector3Subtract(bind[left].translation,bind[left_knee].translation));
    auto lateral=Vector3Subtract(bind[right].translation,bind[left].translation);
    lateral=Vector3Normalize(Vector3Subtract(lateral,Vector3Scale(up,Vector3DotProduct(lateral,up))));
    if(!std::isfinite(up.x) || !std::isfinite(lateral.x) || !std::isfinite(lateral.y) || !std::isfinite(lateral.z) ||
       Vector3LengthSqr(up)<.5F || Vector3LengthSqr(lateral)<.5F) return false;
    (void)facing;
    // Forward is where the toes point; rigs without toes fall back to up x right.
    Vector3 forward=Vector3CrossProduct(up,lateral);
    const int left_foot=find("LeftFoot"),right_foot=find("RightFoot"),left_toe=find("LeftToeBase"),right_toe=find("RightToeBase");
    if(left_foot>=0 && left_toe>=0) {
        auto toes=Vector3Subtract(bind[left_toe].translation,bind[left_foot].translation);
        toes=Vector3Subtract(toes,Vector3Scale(up,Vector3DotProduct(toes,up)));
        if(Vector3LengthSqr(toes)>1e-8F && Vector3DotProduct(Vector3Normalize(toes),forward)<-.5F) forward=Vector3Scale(forward,-1);
    }
    // Sign that swings a hanging limb forward about the lateral axis; knees bend the other way.
    const float forward_sign=Vector3DotProduct(Vector3RotateByQuaternion(Vector3Scale(up,-1),QuaternionFromAxisAngle(lateral,.3F)),forward)>0 ? 1.0F : -1.0F;
    // Sign that turns the left shoulder forward about the up axis (a rifle stance is bladed).
    const float twist_sign=Vector3DotProduct(Vector3RotateByQuaternion(Vector3Scale(lateral,-1),QuaternionFromAxisAngle(up,.3F)),forward)>0 ? 1.0F : -1.0F;
    // Metres per skeleton unit: callers pass it; otherwise an adult's hips stand about 0.92 m up.
    if(!(metres>0) || !std::isfinite(metres)) {
        const int foot=left_foot>=0 ? left_foot : left_knee;
        const float hip_height=Vector3DotProduct(Vector3Subtract(bind[hips].translation,bind[foot].translation),up)*(left_foot>=0 ? 1.0F : 2.0F);
        metres=hip_height>1e-6F ? .92F/hip_height : 1.0F;
    }
    const float unit=1/metres; // skeleton units per metre
    {
        // The ground: soles sit a few centimetres below the balls of the feet.
        float ground=Vector3DotProduct(bind[left_foot>=0 ? left_foot : left_knee].translation,up)-(left_foot>=0 ? .08F : .5F)*unit;
        if(left_toe>=0) ground=std::min(ground,Vector3DotProduct(bind[left_toe].translation,up)-.03F*unit);
        const auto hip=bind[hips].translation;
        origin_=Vector3Add(Vector3Subtract(hip,Vector3Scale(up,Vector3DotProduct(hip,up))),Vector3Scale(up,ground));
        right_=lateral;up_=up;forward_=forward;metres_=metres;
    }

    // Stride follows ground speed so the planted foot keeps pace with the body (leg ~0.9 m).
    float speed=std::isfinite(ground_speed) && ground_speed>=0 ? ground_speed : AnimationController::nominal_speed(action);
    if(action==AnimationAction::Idle && !(std::isfinite(ground_speed) && ground_speed>=0)) speed=0;
    if(dying) speed=0;
    const float cadence=AnimationController::gait_cadence(speed);
    const float run=std::clamp((speed-2.6F)/2.4F,0.0F,1.0F);     // 0 walk .. 1 run blend
    const float crouch=dying ? 0 : std::clamp(body.crouch,0.0F,1.0F);
    // Running covers ground in flight, so its thigh swing is shorter than the stride implies.
    const float swing=std::clamp(std::asin(std::min(1.0F,speed/(3.6F*cadence))),0.0F,.62F-.14F*run)*(1-.35F*crouch);
    const float reach=.12F*run; // runners drive the knee forward more than they extend behind
    const float moving=std::clamp(speed/.6F,0.0F,1.0F);
    const float phase=static_cast<float>(std::fmod(cycle,1.0))*2*PI;
    const float knee_swing=(.45F+1.15F*run)*moving; // flexion while the leg swings through (heel tucks)
    const float knee_stance=(.06F+.18F*run)*moving; // slight give under load
    const float elbow=(.18F+.9F*run)*std::max(moving,.35F);
    // Breathing while still; a zombie stoops and lurches.
    const float breath=std::sin(time*2*PI*.27F)*(1-moving);
    float lean=.14F*run+.32F*crouch+(zombie ? .22F+.06F*std::sin(phase*2) : 0);
    // Aiming: the chest follows the sights up and down; the hips do not.
    const bool armed=body.hold!=BodyHold::None && !dying && !zombie;
    const float pitch=armed ? std::clamp(body.aim_pitch,-1.2F,1.2F) : 0;
    if(armed) lean=.08F*run+.32F*crouch+.06F; // shooters lean into the gun

    // Per leg: thigh angle forward, knee flexion (>=0), for phase offset 0 (left) or PI (right).
    const auto leg=[&](float offset,float limp,float& thigh,float& knee) {
        const float p=phase+offset;
        thigh=(swing*std::sin(p)+reach*std::max(0.0F,std::sin(p)))*limp+crouch*1.05F;
        // Flex peaks mid-swing (thigh moving forward), straight at heel strike, small in stance.
        const float swinging=std::max(0.0F,std::cos(p));
        knee=(knee_swing*swinging*swinging+knee_stance*std::max(0.0F,-std::cos(p)))*limp+crouch*1.85F;
    };
    float left_thigh,left_flex,right_thigh,right_flex;
    leg(0,1,left_thigh,left_flex);leg(PI,zombie ? .55F : 1,right_thigh,right_flex);
    const bool seated=body.seated && !dying;
    if(seated) {
        // Sitting at a wheel: thighs level, shins down to the pedals, a slight lean back.
        left_thigh=right_thigh=1.45F;left_flex=right_flex=1.4F;
        lean=-.08F;
    }
    const float downed=dying || seated ? 0 : std::clamp(body.downed,0.0F,1.0F);
    if(downed>0) {
        // On the knees: thighs down, shins flat behind, the chest slumped over; crawling shuffles.
        const float shuffle=.18F*std::sin(phase)*moving;
        left_thigh+=((.12F+shuffle)-left_thigh)*downed; right_thigh+=((.12F-shuffle)-right_thigh)*downed;
        left_flex+=(1.6F-left_flex)*downed; right_flex+=(1.6F-right_flex)*downed;
        lean+=(1.15F-lean)*downed;
    }
    const float skydive=dying ? 0 : std::clamp(body.skydive,0.0F,1.0F), hang=dying ? 0 : std::clamp(body.hang,0.0F,1.0F);
    const bool airborne=skydive>0 || hang>0;
    if(airborne) {
        // Freefall: thighs a little behind the body, knees bent back. Hanging: legs together,
        // swinging a little forward, knees soft.
        const float kick=.12F*std::sin(time*2.3F);
        const auto blend=[&](float& thigh,float& knee,float side) {
            thigh+=((-.28F+side*.05F)-thigh)*skydive; knee+=((1.0F+kick*side)-knee)*skydive;
            thigh+=((.32F+kick*.5F*side)-thigh)*hang; knee+=((.28F)-knee)*hang;
        };
        blend(left_thigh,left_flex,1);blend(right_thigh,right_flex,-1);
        lean*=1-std::max(skydive,hang);
    }
    if(dying) {
        // Legs give way first.
        const float give=std::clamp(time/.35F,0.0F,1.0F);
        left_thigh=right_thigh=.35F*give;left_flex=right_flex=.75F*give;
    }

    // The gun's frame (skeleton space), built once both shoulders are posed.
    const int left_arm=find("LeftArm"),right_arm=find("RightArm");
    const int left_fore=find("LeftForeArm"),right_fore=find("RightForeArm");
    const int left_hand=find("LeftHand"),right_hand=find("RightHand");
    const bool can_hold=armed && left_arm>=0 && right_arm>=0 && left_fore>=0 && right_fore>=0 && left_hand>=0 && right_hand>=0;
    const auto aim=Vector3Normalize(Vector3Add(Vector3Scale(forward,std::cos(pitch)),Vector3Scale(up,std::sin(pitch))));
    const auto gun_up=Vector3Normalize(Vector3Subtract(Vector3Scale(up,std::cos(pitch)),Vector3Scale(forward,std::sin(pitch))));
    // Each hand: where its wrist goes, where its fingers point (round the grip), where its palm faces.
    Vector3 right_wrist{},left_wrist{},right_fingers{},left_fingers{},right_palm{},left_palm{};
    bool framed=false;

    pose_.assign(model.skeleton.bindPose,model.skeleton.bindPose+model.skeleton.boneCount);
    std::vector<unsigned char> done(static_cast<std::size_t>(model.skeleton.boneCount));
    std::function<bool(int)> build;
    // Where a bone sits once its parent is posed (before its own rotation).
    const auto placed=[&](int i)->Vector3 {
        const int parent=model.skeleton.bones[i].parent;
        if(parent<0) return bind[i].translation;
        const auto inherited=QuaternionMultiply(pose_[parent].rotation,QuaternionInvert(bind[parent].rotation));
        return Vector3Add(pose_[parent].translation,Vector3RotateByQuaternion(Vector3Subtract(bind[i].translation,bind[parent].translation),inherited));
    };
    const auto mix=[&](float r,float u,float f){return Vector3Normalize(Vector3Add(Vector3Add(Vector3Scale(lateral,r),Vector3Scale(gun_up,u)),Vector3Scale(aim,f)));};
    const auto metres_at=[&](Vector3 from,float r,float u,float f){
        return Vector3Add(from,Vector3Scale(Vector3Add(Vector3Add(Vector3Scale(lateral,r),Vector3Scale(gun_up,u)),Vector3Scale(aim,f)),unit));};
    const auto frame=[&]()->bool {
        if(framed) return true;
        const int lp=model.skeleton.bones[left_arm].parent,rp=model.skeleton.bones[right_arm].parent;
        if((lp>=0 && !build(lp)) || (rp>=0 && !build(rp))) return false;
        const auto left_shoulder=placed(left_arm),right_shoulder=placed(right_arm);
        Vector3 origin,right_hold,left_hold;
        if(body.hold==BodyHold::Pistol) {
            // Both arms out, the gun at chin height in front of the chest; the left hand cups the right.
            const auto middle=Vector3Lerp(left_shoulder,right_shoulder,.5F);
            origin=Vector3Add(middle,Vector3Add(Vector3Scale(aim,.42F*unit),Vector3Add(Vector3Scale(up,.01F*unit),Vector3Scale(lateral,.03F*unit))));
            right_hold=metres_at(origin,0,-.05F,-.035F);left_hold=metres_at(origin,-.02F,-.06F,-.03F);
            right_fingers=mix(-.85F,-.35F,.15F);left_fingers=mix(.85F,-.3F,.2F);
            right_palm=Vector3Scale(lateral,-1);left_palm=lateral;
        } else {
            // Stock in the right shoulder pocket, cheek on the stock, support hand under the fore-end.
            // The shoulder joint sits deep in the chest; the pocket is on its front, a little lower.
            const auto pocket=Vector3Add(right_shoulder,Vector3Add(Vector3Scale(lateral,-.08F*unit),Vector3Add(Vector3Scale(up,-.11F*unit),Vector3Scale(aim,.10F*unit))));
            origin=Vector3Add(pocket,Vector3Add(Vector3Scale(gun_up,.035F*unit),Vector3Scale(aim,body.stock*.85F*unit)));
            right_hold=metres_at(origin,0,-.06F,-.035F);left_hold=metres_at(origin,0,-.03F,body.support);
            right_fingers=mix(-.8F,-.45F,.2F);left_fingers=mix(.65F,.15F,.55F);
            right_palm=Vector3Scale(lateral,-1);left_palm=gun_up;
        }
        // The wrist sits a hand's breadth back from where the hand closes.
        right_wrist=Vector3Subtract(right_hold,Vector3Scale(right_fingers,.075F*unit));
        left_wrist=Vector3Subtract(left_hold,Vector3Scale(left_fingers,.075F*unit));
        grip_=origin;aim_=aim;gun_up_=gun_up;holding_=true;framed=true;
        return true;
    };
    // Two-bone reach: point the upper arm at the elbow, the forearm at the wrist.
    const auto reach_for=[&](int i,Transform& out,const Quaternion& rest_rotation,bool upper,bool right_side)->bool {
        if(!frame()) return false;
        const int elbow_bone=right_side ? right_fore : left_fore,hand_bone=right_side ? right_hand : left_hand;
        const auto target=right_side ? right_wrist : left_wrist;
        const auto turn=[&](int child,Vector3 desired) {
            const auto current=Vector3RotateByQuaternion(Vector3Subtract(bind[child].translation,bind[i].translation),
                QuaternionMultiply(out.rotation,QuaternionInvert(rest_rotation)));
            if(Vector3LengthSqr(current)>1e-10F && Vector3LengthSqr(desired)>1e-10F)
                out.rotation=QuaternionNormalize(QuaternionMultiply(QuaternionFromVector3ToVector3(Vector3Normalize(current),Vector3Normalize(desired)),out.rotation));
        };
        if(!upper) { turn(hand_bone,Vector3Subtract(target,out.translation)); return true; }
        const float a=Vector3Distance(bind[elbow_bone].translation,bind[i].translation);
        const float b=Vector3Distance(bind[hand_bone].translation,bind[elbow_bone].translation);
        auto to=Vector3Subtract(target,out.translation);
        const float d=std::clamp(Vector3Length(to),std::abs(a-b)+1e-4F,(a+b)*.999F);
        if(Vector3LengthSqr(to)<1e-10F) return true;
        const auto n=Vector3Normalize(to);
        // Elbows drop and flare out; the trigger arm's flares more.
        auto pole=Vector3Add(Vector3Scale(gun_up,-1),Vector3Add(Vector3Scale(lateral,right_side ? .75F : -.45F),Vector3Scale(aim,-.15F)));
        pole=Vector3Subtract(pole,Vector3Scale(n,Vector3DotProduct(pole,n)));
        if(Vector3LengthSqr(pole)<1e-8F) pole=Vector3Scale(up,-1);
        pole=Vector3Normalize(pole);
        const float along=(a*a-b*b+d*d)/(2*d),side=std::sqrt(std::max(0.0F,a*a-along*along));
        const auto elbow_at=Vector3Add(out.translation,Vector3Add(Vector3Scale(n,along),Vector3Scale(pole,side)));
        turn(elbow_bone,Vector3Subtract(elbow_at,out.translation));
        return true;
    };
    build=[&](int i) {
        if(done[i]==2)return true;
        if(done[i]==1)return false;
        done[i]=1;
        const int parent=model.skeleton.bones[i].parent;
        if(parent>=model.skeleton.boneCount || parent==i || parent<-1)return false;
        auto& out=pose_[i];const auto rest=model.skeleton.bindPose[i];
        Quaternion inherited=QuaternionIdentity();
        if(parent>=0) {
            if(!build(parent))return false;
            inherited=QuaternionMultiply(pose_[parent].rotation,QuaternionInvert(model.skeleton.bindPose[parent].rotation));
            out.translation=Vector3Add(pose_[parent].translation,Vector3RotateByQuaternion(Vector3Subtract(rest.translation,model.skeleton.bindPose[parent].translation),inherited));
            out.rotation=QuaternionMultiply(inherited,rest.rotation);
        }
        const auto& bone=model.skeleton.bones[i];
        const bool left_upper=named(bone,"LeftArm"),right_upper=named(bone,"RightArm");
        const bool left_lower=named(bone,"LeftForeArm"),right_lower=named(bone,"RightForeArm");
        if(can_hold && (left_upper || right_upper || left_lower || right_lower)) {
            if(!reach_for(i,out,rest.rotation,left_upper || right_upper,right_upper || right_lower)) return false;
            done[i]=2;return true;
        }
        if(can_hold && (i==left_hand || i==right_hand) && frame()) {
            // Close the hand round the grip: fingers across it, palm against it. A T-pose rig's
            // hand points along its forearm with the palm down.
            const bool right_side=i==right_hand;
            const auto turned=QuaternionMultiply(out.rotation,QuaternionInvert(rest.rotation));
            const auto along=Vector3RotateByQuaternion(Vector3Normalize(Vector3Subtract(bind[i].translation,bind[right_side ? right_fore : left_fore].translation)),turned);
            const auto fingers=right_side ? right_fingers : left_fingers;
            if(Vector3LengthSqr(along)>1e-10F) {
                const auto bend=QuaternionFromVector3ToVector3(Vector3Normalize(along),fingers);
                out.rotation=QuaternionNormalize(QuaternionMultiply(bend,out.rotation));
                auto palm=Vector3RotateByQuaternion(Vector3Scale(up,-1),QuaternionMultiply(bend,turned));
                auto wanted=right_side ? right_palm : left_palm;
                palm=Vector3Subtract(palm,Vector3Scale(fingers,Vector3DotProduct(palm,fingers)));
                wanted=Vector3Subtract(wanted,Vector3Scale(fingers,Vector3DotProduct(wanted,fingers)));
                if(Vector3LengthSqr(palm)>1e-8F && Vector3LengthSqr(wanted)>1e-8F)
                    out.rotation=QuaternionNormalize(QuaternionMultiply(QuaternionFromVector3ToVector3(Vector3Normalize(palm),Vector3Normalize(wanted)),out.rotation));
            }
            done[i]=2;return true;
        }
        if(left_upper || right_upper) {
            // T-pose arms hang at the sides first, a little away from the hips.
            const int elbow_bone=find(left_upper ? "LeftForeArm":"RightForeArm");
            if(elbow_bone>=0) {
                const auto direction=Vector3RotateByQuaternion(Vector3Subtract(model.skeleton.bindPose[elbow_bone].translation,rest.translation),inherited);
                const float side=left_upper ? -1.0F : 1.0F;
                auto hang_to=Vector3Normalize(Vector3Add(Vector3Scale(up,-1),Vector3Scale(lateral,side*(.13F+.02F*breath))));
                // Spread out to the sides in freefall; up to the toggles under a canopy.
                if(skydive>0) hang_to=Vector3Normalize(Vector3Lerp(hang_to,Vector3Normalize(Vector3Add(Vector3Scale(lateral,side*.92F),Vector3Scale(up,.38F))),skydive));
                if(hang>0) hang_to=Vector3Normalize(Vector3Lerp(hang_to,Vector3Normalize(Vector3Add(Vector3Scale(lateral,side*.32F),Vector3Scale(up,.95F))),hang));
                // Hands up: both arms straight up, a little apart. Cuffed: in close to the body.
                const float hands_up=dying ? 0 : std::clamp(body.hands_up,0.0F,1.0F),cuffed=dying ? 0 : std::clamp(body.cuffed,0.0F,1.0F);
                if(hands_up>0) hang_to=Vector3Normalize(Vector3Lerp(hang_to,Vector3Normalize(Vector3Add(Vector3Scale(lateral,side*.42F),Vector3Scale(up,.9F))),hands_up));
                if(cuffed>0) hang_to=Vector3Normalize(Vector3Lerp(hang_to,Vector3Normalize(Vector3Add(Vector3Scale(up,-1),Vector3Scale(lateral,side*.05F))),cuffed));
                if(Vector3LengthSqr(direction)>.00001F)out.rotation=QuaternionMultiply(QuaternionFromVector3ToVector3(Vector3Normalize(direction),hang_to),out.rotation);
            }
        }
        // All angles below are "forward" angles about the lateral axis: they swing a limb that
        // hangs down forward. The spine points up, so leaning the chest forward is negative.
        float angle=0;
        if(named(bone,"Spine"))angle=-lean*.6F+.012F*breath;
        if(named(bone,"Spine1"))angle=-lean*.4F+pitch*.3F;
        if(named(bone,"Spine2"))angle=pitch*.3F+.01F*breath;
        if(named(bone,"Neck"))angle=pitch*.25F+(zombie ? .25F : crouch*.2F);
        if(named(bone,"LeftUpLeg"))angle=left_thigh;
        if(named(bone,"RightUpLeg"))angle=right_thigh;
        if(named(bone,"LeftLeg"))angle=-left_flex;
        if(named(bone,"RightLeg"))angle=-right_flex;
        // Feet stay near level: undo the shin's pitch, letting the toes lift a little in swing.
        if(named(bone,"LeftFoot"))angle=-(left_thigh-left_flex)*.85F;
        if(named(bone,"RightFoot"))angle=-(right_thigh-right_flex)*.85F;
        if(zombie) {
            // Arms reach out in front; an attack chops down and back up.
            const float chop=attacking ? std::sin(std::clamp(time/.45F,0.0F,1.0F)*PI) : 0;
            const float sway=.08F*std::sin(phase+(left_upper ? 0 : 1.7F))+.04F*std::sin(time*1.3F);
            if(left_upper || right_upper) angle=1.3F+sway-.75F*chop+lean;
            if(left_lower || right_lower) angle=.25F+.35F*chop;
        } else {
            // Arms swing against the legs (left arm with the right leg); elbows bend forward.
            if(left_upper)angle=-left_thigh*(.75F+.6F*run)+lean+(attacking ? .9F*std::sin(std::clamp(time/.4F,0.0F,1.0F)*PI) : 0);
            if(right_upper)angle=-right_thigh*(.75F+.6F*run)+lean+crouch*.3F;
            if(left_lower || right_lower)angle=elbow+crouch*.3F+(dying ? .5F : 0);
            // A driver's hands rest on the wheel, turning it a little now and then.
            if(seated && (left_upper || right_upper)) angle=.85F+.05F*std::sin(time*.7F+(left_upper ? 0 : 2));
            if(seated && (left_lower || right_lower)) angle=.75F;
            // In the air the arms hold their spread (or the toggles) instead of swinging.
            if(airborne && (left_upper || right_upper)) angle*=1-std::max(skydive,hang);
            if(airborne && (left_lower || right_lower)) angle=angle*(1-std::max(skydive,hang))+.35F*skydive+.25F*hang;
            // Hands up: the arms held straight (no swing). Cuffed: the arms back, wrists together.
            if(!dying && body.hands_up>0) {
                const float h=std::clamp(body.hands_up,0.0F,1.0F);
                if(left_upper || right_upper || left_lower || right_lower) angle*=1-h;
            }
            // Down: the arms hang straight to the ground under the slumped chest.
            if(downed>0) {
                if(left_upper || right_upper) angle*=1-downed;
                if(left_lower || right_lower) angle=angle*(1-downed)+.1F*downed;
            }
            if(!dying && body.cuffed>0) {
                const float c=std::clamp(body.cuffed,0.0F,1.0F);
                if(left_upper || right_upper) angle=angle*(1-c)-.45F*c;
                if(left_lower || right_lower) angle=angle*(1-c)+.7F*c;
            }
        }
        if(angle!=0)out.rotation=QuaternionNormalize(QuaternionMultiply(QuaternionFromAxisAngle(lateral,angle*forward_sign),out.rotation));
        // Hips sway with the stride and the chest counters it; a rifle stance turns the chest.
        float turn=0;
        if(named(bone,"Hips"))turn=.09F*std::sin(phase)*moving*(1-.5F*run);
        if(named(bone,"Spine1"))turn=-.09F*std::sin(phase)*moving*(1-.5F*run);
        if(named(bone,"Spine2") && armed && body.hold==BodyHold::Shouldered)turn=.32F;
        if(named(bone,"Neck") && armed && body.hold==BodyHold::Shouldered)turn=-.28F;
        if(turn!=0)out.rotation=QuaternionNormalize(QuaternionMultiply(QuaternionFromAxisAngle(up,turn*twist_sign),out.rotation));
        done[i]=2;return true;
    };
    for(int i=0;i<model.skeleton.boneCount;++i)if(!build(i))return false;
    // Plant the lowest foot: a swung straight leg is shorter, so drop the body instead of letting
    // both feet float (and never push a foot through the ground).
    float lowest=1e9F;
    for(const int b:{left_foot,right_foot,left_toe,right_toe}) if(b>=0)
        lowest=std::min(lowest,Vector3DotProduct(Vector3Subtract(pose_[b].translation,bind[b].translation),up));
    if(left_foot<0 && right_foot<0) {
        // No feet: use the ends of the shins.
        for(const int b:{left_knee,right_knee}) lowest=std::min(lowest,Vector3DotProduct(Vector3Subtract(pose_[b].translation,bind[b].translation),up));
    }
    if(!seated && !airborne && lowest<1e8F && std::abs(lowest)>1e-7F) {
        for(auto& p:pose_) p.translation=Vector3Subtract(p.translation,Vector3Scale(up,lowest));
        grip_=Vector3Subtract(grip_,Vector3Scale(up,lowest));
    }
    if(airborne && (body.body_pitch!=0 || body.body_roll!=0) && hips>=0) {
        // Tilt the whole body about the hips: forward (belly-down) and into a bank.
        const auto pivot=pose_[hips].translation;
        auto q=QuaternionFromAxisAngle(lateral,body.body_pitch);
        if(Vector3DotProduct(Vector3RotateByQuaternion(up,q),forward)*body.body_pitch<0) q=QuaternionFromAxisAngle(lateral,-body.body_pitch);
        q=QuaternionNormalize(QuaternionMultiply(QuaternionFromAxisAngle(forward,body.body_roll),q));
        for(auto& p:pose_) {
            p.translation=Vector3Add(pivot,Vector3RotateByQuaternion(Vector3Subtract(p.translation,pivot),q));
            p.rotation=QuaternionNormalize(QuaternionMultiply(q,p.rotation));
        }
        grip_=Vector3Add(pivot,Vector3RotateByQuaternion(Vector3Subtract(grip_,pivot),q));
    }
    if(dying) {
        // Topple backwards about the heels and come to rest on the back.
        const float t=std::clamp((time-.15F)/.65F,0.0F,1.0F);
        const float fall=t*t*(3-2*t);
        const int heel=left_foot>=0 ? left_foot : left_knee;
        const auto pivot=Vector3Add(Vector3Lerp(pose_[heel].translation,pose_[right_foot>=0 ? right_foot : right_knee].translation,.5F),
            Vector3Scale(forward,-.05F*unit));
        auto q=QuaternionFromAxisAngle(lateral,PI*.5F*fall);
        if(Vector3DotProduct(Vector3RotateByQuaternion(up,q),forward)>0) q=QuaternionFromAxisAngle(lateral,-PI*.5F*fall);
        const auto lift=Vector3Scale(up,.13F*unit*fall);
        for(auto& p:pose_) {
            p.translation=Vector3Add(Vector3Add(pivot,Vector3RotateByQuaternion(Vector3Subtract(p.translation,pivot),q)),lift);
            p.rotation=QuaternionNormalize(QuaternionMultiply(q,p.rotation));
        }
    }
    // The crown of the head (a hat sits there): the head-top joint where the rig has one, else
    // a little above the head joint, turned with the head.
    crown_valid_=false;
    if(const int head=find("Head");head>=0) {
        const auto turn=QuaternionMultiply(pose_[head].rotation,QuaternionInvert(bind[head].rotation));
        const int tip=find("HeadTop_End");
        crown_=tip>=0 ? pose_[tip].translation :
            Vector3Add(pose_[head].translation,Vector3RotateByQuaternion(Vector3Scale(up,.17F*unit),turn));
        crown_up_=Vector3RotateByQuaternion(up,turn);
        crown_forward_=Vector3RotateByQuaternion(forward,turn);
        crown_valid_=std::isfinite(crown_.x) && std::isfinite(crown_up_.x) && std::isfinite(crown_forward_.x);
    }
    return true;
}
Vector3 SkeletalGait::body_direction(Vector3 d) const {
    return {Vector3DotProduct(d,right_),Vector3DotProduct(d,up_),Vector3DotProduct(d,forward_)};
}
Vector3 SkeletalGait::body_point(Vector3 p) const {
    return Vector3Scale(body_direction(Vector3Subtract(p,origin_)),metres_);
}
void SkeletalGait::to_skin_space(const Model& model,const Matrix& skin) {
    // raylib skins with inverse(bind) then pose, both in bone space, but applies the glTF mesh
    // node's world transform (e.g. a +90 X / 0.01 Armature) to the vertices only. Conjugate each
    // pose so the deformation happens in the space the vertices live in:
    //   pose' = bind -> skin^-1 -> bind^-1 -> pose -> skin   (applied left to right)
    const auto inverse=MatrixInvert(skin);
    const auto matrix=[](const Transform& t){return MatrixMultiply(MatrixMultiply(MatrixScale(t.scale.x,t.scale.y,t.scale.z),QuaternionToMatrix(t.rotation)),MatrixTranslate(t.translation.x,t.translation.y,t.translation.z));};
    for(int i=0;i<model.skeleton.boneCount && i<static_cast<int>(pose_.size());++i) {
        const auto bind=matrix(model.skeleton.bindPose[i]);
        const auto result=MatrixMultiply(MatrixMultiply(MatrixMultiply(MatrixMultiply(bind,inverse),MatrixInvert(bind)),matrix(pose_[i])),skin);
        Transform t{};MatrixDecompose(result,&t.translation,&t.rotation,&t.scale);
        pose_[i]=t;
    }
}
bool SkeletalGait::apply(Model& model,AnimationAction action,double cycle,float ground_speed,float facing,const Matrix* skin,
                         const BodyPose& body,float time,float metres) {
    if(!sample(model,action,cycle,ground_speed,facing,body,time,metres))return false;
    if(skin) to_skin_space(model,*skin);
    auto* frame=pose_.data();ModelAnimation animation{};animation.boneCount=model.skeleton.boneCount;
    animation.keyframeCount=1;animation.keyframePoses=&frame;
    UpdateModelAnimation(model,animation,0);return true;
}
}
