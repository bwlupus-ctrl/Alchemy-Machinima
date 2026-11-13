"""Checked slot-only Classic extension; all other bytes come from the frozen blob."""


def extend_classic(original):
    def once(old, new):
        nonlocal original
        assert original.count(old) == 1, old
        original = original.replace(old, new)
    once('uniform sampler2D   normalMap;', '''// The 16-slot variant is admitted only on >=32 fragment texture units.
#ifdef SPOT_SHADOWS_16
#define SPOT_SHADOW_COUNT 16
#define SPOT_SHADOW_MATRIX_COUNT 20
#else
#define SPOT_SHADOW_COUNT 10
#define SPOT_SHADOW_MATRIX_COUNT 14
#endif

uniform sampler2D   normalMap;''')
    once('uniform sampler2DShadow shadowMap13;', 'uniform sampler2DShadow shadowMap13;\n#ifdef SPOT_SHADOWS_16\n'+''.join('uniform sampler2DShadow shadowMap%d;\n'%i for i in range(14,20))+'#endif')
    once('uniform mat4 shadow_matrix[14]; // [BDMerge NSpot] 4 sun + up to 10 spot', 'uniform mat4 shadow_matrix[SPOT_SHADOW_MATRIX_COUNT]; // [BDMerge NSpot] 4 sun + up to 16 spot')
    once('uniform float spot_shadow_softness[10];','uniform float spot_shadow_softness[SPOT_SHADOW_COUNT];')
    assert original.count('this 10-case GLSL dispatch') == 2
    original = original.replace('this 10-case GLSL dispatch', 'this 16-case GLSL dispatch')
    old='''            else
            {
                lpos = shadow_matrix[13]*spos;
                shadow += pcfSpotShadow(shadowMap13, lpos, 0.8, spos.xy, spot_shadow_softness[9])*w;
            }'''
    new=old.replace('else\n','else if (index == 9)\n',1)+'\n#ifdef SPOT_SHADOWS_16\n'
    for slot in range(10,16):
        new+=old.replace('else\n', 'else if (index == %d)\n'%slot if slot<15 else 'else\n',1).replace('[13]','[%d]'%(slot+4)).replace('shadowMap13','shadowMap%d'%(slot+4)).replace('[9]','[%d]'%slot)+'\n'
    once(old,new+'#endif')
    old='    return pcfSpotShadowConservative(shadowMap13, shadow_matrix[13] * spos);'
    once(old,'#ifdef SPOT_SHADOWS_16\n'+''.join('    if (index == %d) return pcfSpotShadowConservative(shadowMap%d, shadow_matrix[%d] * spos);\n'%(i,i+4,i+4) for i in range(9,15))+'    return pcfSpotShadowConservative(shadowMap19, shadow_matrix[19] * spos);\n#else\n'+old+'\n#endif')
    return original


def reverse_classic_spot16(companion, frozen):
    # Work in bytes: newline changes and unrelated whitespace also fail identity.
    expected=extend_classic(frozen.decode('utf-8')).encode('utf-8')
    assert companion == expected, 'Classic companion differs outside the exact slot-only transform'
    return frozen


def check_classic_extension(root, frozen):
    path=root/'indra/newview/app_settings/shaders/class1/deferred/projectorVolumetricClassicShadowUtil16.glsl'
    blob=path.read_bytes()
    assert reverse_classic_spot16(blob,frozen) == frozen
    # Mutate arithmetic in both the inherited body and the new dispatch.
    for old,new in [(b'0.8',b'0.7'),(b'shadow_matrix[19]',b'shadow_matrix[18]'),(b'shadow +=',b'shadow -=')]:
        assert old in blob
        try: reverse_classic_spot16(blob.replace(old,new,1),frozen)
        except AssertionError: pass
        else: raise AssertionError('Classic arithmetic mutant escaped: '+repr(old))
    return True
