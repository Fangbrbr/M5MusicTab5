/**
 * @file tarot.h
 * @brief 塔罗牌数据（78 张大阿卡纳与小阿卡纳）
 */

#ifndef TAROT_H
#define TAROT_H

typedef struct {
    const char* name;      /* 牌名 */
    const char* upright;   /* 正位释义 */
    const char* reversed;  /* 逆位释义 */
} tarot_card_t;

/* 大阿卡纳 22 张 */
static const tarot_card_t s_tarot_major_arcana[] = {
    {"愚者", "象征天真无邪的出发与无限可能，代表全新旅程的开端，蕴含自由精神与自发性的能量，鼓励以开放心态迎接未知。", "代表鲁莽冲动或逃避现实的倾向，暗示对风险的忽视与盲目乐观，显示决策根基不稳与缺乏实际规划的状态。"},
    {"魔术师", "象征创造力与显化的力量，代表将意念转化为现实的能力，蕴含专注与自信的意志，显示资源充沛与时机成熟。", "代表欺骗、操纵或能力滥用的阴影，暗示缺乏真诚与方向迷失，显示潜能被浪费或意图不纯的状态。"},
    {"女祭司", "象征直觉、神秘与内在智慧，代表潜意识深处的真理与未显化的知识，蕴含静观与等待时机的耐心。", "代表直觉受阻或秘密被掩盖的状态，暗示过度理性压抑本能，显示内在声音被忽视与自我欺骗的倾向。"},
    {"皇后", "象征丰饶、滋养与母性的创造力，代表自然界的丰盛与感官的愉悦，蕴含孕育与成长的温柔力量。", "代表过度依赖或创造力受阻的状态，暗示对物质的执念与情感的过度掌控，显示丰盛能量的停滞与枯竭。"},
    {"皇帝", "象征权威、秩序与结构化的力量，代表稳定的统治与理性的规划，蕴含父性的保护与制度的建立。", "代表独裁、僵化或权威滥用的阴影，暗示过度控制与缺乏弹性，显示规则成为束缚而非支撑的倾向。"},
    {"教皇", "象征传统、信仰与精神上的指引，代表社会规范与集体智慧的价值，蕴含仪式与传承的深层意义。", "代表反叛传统或盲目信仰的状态，暗示对既定规则的质疑与精神上的孤立，显示教条主义带来的局限。"},
    {"恋人", "象征爱情、和谐与重要的抉择，代表价值观的契合与灵魂的结合，蕴含亲密关系中的真诚与信任。", "代表关系失衡或价值观冲突的状态，暗示不忠、逃避承诺与内心的分裂，显示抉择中的犹豫与混乱。"},
    {"战车", "象征意志力、胜利与掌控的力量，代表通过专注与决心克服障碍，蕴含自我驾驭与目标达成的动能。", "代表失控或方向迷失的状态，暗示意志力涣散与内外冲突的加剧，显示过度用力导致的崩溃与挫败。"},
    {"力量", "象征内在勇气、耐心与温柔的坚韧，代表以柔克刚的智慧与情绪的掌控，蕴含非暴力的深层力量。", "代表软弱、恐惧或情绪失控的状态，暗示内在力量的枯竭与自信的丧失，显示以暴制暴或压抑情感的倾向。"},
    {"隐士", "象征内省、孤独与智慧的寻求，代表退隐以寻找内在真理的旅程，蕴含静默中的启示与精神的指引。", "代表孤立过度或逃避社会的倾向，暗示在孤独中迷失与拒绝连接，显示自我封闭导致的偏执与疏离。"},
    {"命运之轮", "象征命运的转折、循环与机遇的降临，代表生命周期的自然律动与不可控的变化，蕴含顺应天时的智慧。", "代表厄运、阻碍或抗拒变化的状态，暗示循环中断与机遇的错失，显示对命运安排的过度挣扎与执念。"},
    {"正义", "象征公正、平衡与因果的法则，代表理性判断与道德责任的承担，蕴含真相大白与公平裁决的力量。", "代表不公、偏见或逃避责任的状态，暗示失衡的判决与道德标准的扭曲，显示因果报应的延迟或错位。"},
    {"倒吊人", "象征牺牲、暂停与视角的转换，代表通过臣服获得启示的智慧，蕴含等待与放手以换取更高理解的耐心。", "代表无谓的牺牲或抗拒改变的状态，暗示固执于旧有视角与无效的停滞，显示拖延与逃避成长的倾向。"},
    {"死神", "象征结束、转化与重生的必然，代表旧有阶段的终结与新循环的开始，蕴含放下过去以迎接蜕变的勇气。", "代表停滞、抗拒结束或转化受阻的状态，暗示对改变的恐惧与旧模式的纠缠，显示新生被延缓的僵局。"},
    {"节制", "象征调和、中庸与炼金般的融合，代表对立元素的平衡与耐心的调和，蕴含温和节制带来的内在和谐。", "代表失衡、过度或缺乏调和的状态，暗示极端行为与自我放纵，显示内在对立面的冲突与整合失败。"},
    {"恶魔", "象征束缚、欲望与物质主义的陷阱，代表被本能与成瘾控制的阴影面，蕴含对自由意志的深刻考验。", "代表解脱、觉醒或打破枷锁的状态，暗示摆脱依赖与重获自由的可能，显示阴影面被正视与转化的契机。"},
    {"高塔", "象征剧变、崩塌与突发的启示，代表虚假结构的瓦解与真相的强制揭露，蕴含破坏中隐藏的重生契机。", "代表逃避剧变或内在崩塌的状态，暗示对变革的抗拒与自我欺骗的维持，显示灾难被延缓但未被化解。"},
    {"星星", "象征希望、疗愈与灵性的指引，代表黑暗之后的宁静与信心的恢复，蕴含宇宙恩赐与灵感灌注的温柔。", "代表绝望、失去信心或灵感枯竭的状态，暗示希望的空洞化与自我怀疑的蔓延，显示与宇宙能量的疏离。"},
    {"月亮", "象征幻觉、潜意识与未知的恐惧，代表直觉深处的阴影与梦境的启示，蕴含穿越迷雾以寻找真相的旅程。", "代表恐惧消散或幻觉破灭的状态，暗示潜意识的混乱被理清与迷雾的散去，显示隐藏的真相逐渐浮现。"},
    {"太阳", "象征成功、喜悦与生命力的绽放，代表清晰的视野与纯真的快乐，蕴含光明照亮一切阴霾的积极能量。", "代表暂时的阴霾、喜悦受阻或自我膨胀的状态，暗示成功中的盲点与过度乐观，显示光明被暂时遮蔽。"},
    {"审判", "象征觉醒、重生与因果的清算，代表内心的召唤与过去的总结，蕴含宽恕与自我接纳带来的精神升华。", "代表自我怀疑、逃避审判或拒绝觉醒的状态，暗示对过往的执念与内心的苛责，显示重生契机被忽视。"},
    {"世界", "象征圆满、完成与整合的境界，代表旅程的终点与成就的实现，蕴含宇宙和谐与自我实现的至高状态。", "代表未完成的圆满或整合受阻的状态，暗示在终点前的停滞与对完美的执念，显示循环尚未真正闭合。"},
};

/* 大阿卡纳 22 张（英文） */
static const tarot_card_t s_tarot_major_arcana_en[] = {
    {"The Fool", "Symbolizes an innocent departure and limitless possibility; it marks the beginning of a brand-new journey, carrying the energy of freedom and spontaneity, and encourages embracing the unknown with an open heart.", "Indicates recklessness, impulsiveness, or a tendency to escape reality; it suggests disregard for risk and blind optimism, revealing unstable decision-making and a lack of practical planning."},
    {"The Magician", "Symbolizes the power of creativity and manifestation; it represents the ability to turn ideas into reality, carrying focused and confident willpower, and shows that resources are abundant and the timing is ripe.", "Represents the shadow of deception, manipulation, or misuse of ability; it suggests a lack of sincerity and a loss of direction, revealing wasted potential or impure intentions."},
    {"The High Priestess", "Symbolizes intuition, mystery, and inner wisdom; it represents truths hidden in the depths of the subconscious and knowledge not yet manifested, carrying the patience to observe quietly and wait for the right moment.", "Represents blocked intuition or concealed secrets; it suggests excessive rationality suppressing instinct, revealing a tendency to ignore the inner voice and deceive oneself."},
    {"The Empress", "Symbolizes abundance, nourishment, and maternal creativity; it represents the bounty of nature and sensory pleasure, carrying the gentle power of gestation and growth.", "Represents over-dependence or blocked creativity; it suggests attachment to material things and excessive control over emotions, revealing stagnation and depletion of the energy of abundance."},
    {"The Emperor", "Symbolizes authority, order, and structured power; it represents stable rule and rational planning, carrying paternal protection and the establishment of institutions.", "Represents the shadow of dictatorship, rigidity, or abuse of authority; it suggests excessive control and inflexibility, revealing rules that bind rather than support."},
    {"The Hierophant", "Symbolizes tradition, faith, and spiritual guidance; it represents the value of social norms and collective wisdom, carrying the deep meaning of ritual and inheritance.", "Represents rebellion against tradition or blind belief; it suggests questioning established rules and spiritual isolation, revealing the limitations imposed by dogmatism."},
    {"The Lovers", "Symbolizes love, harmony, and important choices; it represents the alignment of values and the union of souls, carrying sincerity and trust in intimate relationships.", "Represents imbalanced relationships or conflicting values; it suggests infidelity, avoidance of commitment, and inner division, revealing hesitation and confusion in decision-making."},
    {"The Chariot", "Symbolizes willpower, victory, and the power of control; it represents overcoming obstacles through focus and determination, carrying the momentum of self-mastery and goal achievement.", "Represents loss of control or loss of direction; it suggests scattered willpower and intensifying internal and external conflict, revealing breakdown and frustration caused by excessive force."},
    {"Strength", "Symbolizes inner courage, patience, and gentle resilience; it represents the wisdom of overcoming force with softness and mastery over emotions, carrying deep non-violent strength.", "Represents weakness, fear, or emotional loss of control; it suggests depleted inner strength and a loss of confidence, revealing a tendency to meet violence with violence or to suppress feelings."},
    {"The Hermit", "Symbolizes introspection, solitude, and the search for wisdom; it represents a journey of withdrawal to find inner truth, carrying revelation in silence and spiritual guidance.", "Represents excessive isolation or a tendency to escape society; it suggests becoming lost in loneliness and refusing connection, revealing prejudice and alienation caused by self-enclosure."},
    {"Wheel of Fortune", "Symbolizes turning points of fate, cycles, and the arrival of opportunity; it represents the natural rhythm of life's cycles and uncontrollable change, carrying the wisdom of flowing with the times.", "Represents misfortune, obstruction, or resistance to change; it suggests interrupted cycles and missed opportunities, revealing excessive struggle and attachment against the arrangements of fate."},
    {"Justice", "Symbolizes fairness, balance, and the law of cause and effect; it represents rational judgment and the acceptance of moral responsibility, carrying the power of truth revealed and fair verdicts.", "Represents injustice, bias, or evasion of responsibility; it suggests unbalanced judgment and distorted moral standards, revealing delayed or misplaced karmic consequences."},
    {"The Hanged Man", "Symbolizes sacrifice, pause, and a shift in perspective; it represents the wisdom of gaining enlightenment through surrender, carrying the patience to wait and let go in exchange for higher understanding.", "Represents pointless sacrifice or resistance to change; it suggests clinging to old perspectives and ineffective stagnation, revealing procrastination and an avoidance of growth."},
    {"Death", "Symbolizes endings, transformation, and the inevitability of rebirth; it represents the close of an old phase and the start of a new cycle, carrying the courage to let go of the past and embrace metamorphosis.", "Represents stagnation, resistance to endings, or blocked transformation; it suggests fear of change and entanglement with old patterns, revealing a deadlock in which renewal is delayed."},
    {"Temperance", "Symbolizes harmony, moderation, and alchemical blending; it represents the balancing of opposing elements and patient reconciliation, carrying the inner harmony brought by gentle restraint.", "Represents imbalance, excess, or a lack of moderation; it suggests extreme behavior and self-indulgence, revealing conflict between inner opposites and failed integration."},
    {"The Devil", "Symbolizes bondage, desire, and the trap of materialism; it represents the shadow side controlled by instinct and addiction, carrying a profound test of free will.", "Represents liberation, awakening, or the breaking of chains; it suggests the possibility of escaping dependence and regaining freedom, revealing an opportunity for the shadow side to be faced and transformed."},
    {"The Tower", "Symbolizes upheaval, collapse, and sudden revelation; it represents the disintegration of false structures and the forced exposure of truth, carrying an opportunity for rebirth hidden within destruction.", "Represents avoidance of upheaval or inner collapse; it suggests resistance to change and the maintenance of self-deception, revealing a disaster postponed but not resolved."},
    {"The Star", "Symbolizes hope, healing, and spiritual guidance; it represents tranquility after darkness and the restoration of faith, carrying the gentleness of cosmic blessing and inspired infusion.", "Represents despair, loss of faith, or dried-up inspiration; it suggests the hollowing of hope and the spread of self-doubt, revealing estrangement from the energy of the universe."},
    {"The Moon", "Symbolizes illusion, the subconscious, and fear of the unknown; it represents shadows deep within intuition and the revelations of dreams, carrying a journey through the mist in search of truth.", "Represents the dissipation of fear or the shattering of illusion; it suggests the clearing of subconscious confusion and the lifting of the mist, revealing hidden truths gradually surfacing."},
    {"The Sun", "Symbolizes success, joy, and the blossoming of vitality; it represents clear vision and innocent happiness, carrying the positive energy of light dispelling all gloom.", "Represents temporary clouds, blocked joy, or self-inflation; it suggests blind spots within success and excessive optimism, revealing the light temporarily obscured."},
    {"Judgement", "Symbolizes awakening, rebirth, and the settling of karma; it represents an inner calling and a summing-up of the past, carrying the spiritual sublimation brought by forgiveness and self-acceptance.", "Represents self-doubt, avoidance of judgment, or refusal to awaken; it suggests attachment to the past and inner harshness, revealing an opportunity for rebirth being overlooked."},
    {"The World", "Symbolizes completion, fulfillment, and integration; it represents the end of a journey and the realization of achievement, carrying the supreme state of cosmic harmony and self-realization.", "Represents unfinished completion or blocked integration; it suggests stagnation just before the finish line and attachment to perfection, revealing that the cycle has not truly closed."},
};

/* 权杖 14 张 */
static const tarot_card_t s_tarot_wands[] = {
    {"权杖一", "象征创造力与激情的初始火花，代表全新行动计划的启动，蕴含火元素纯粹的能量与勇往直前的冲劲。", "代表行动力的延迟或创意受阻的状态，暗示能量分散与缺乏方向，显示开端被拖延或热情迅速消退。"},
    {"权杖二", "象征对未来的规划与可能性的权衡，代表在已知与未知之间的抉择，蕴含远见与策略性的思考。", "代表恐惧改变或决策瘫痪的状态，暗示过度谨慎导致错失良机，显示规划缺乏信心与行动的勇气。"},
    {"权杖三", "象征初步成果的显现与视野的拓展，代表合作带来的进展与远方的机遇，蕴含成长与探索的期待。", "代表合作受阻或计划延迟的状态，暗示团队内部的冲突与目标的分歧，显示扩展遭遇意外的障碍。"},
    {"权杖四", "象征稳定基础的建立与成果的庆祝，代表和谐环境中的安全感与归属感，蕴含社群支持与阶段性胜利。", "代表不稳定或缺乏归属感的状态，暗示表面的和谐掩盖内在冲突，显示基础不牢与过渡安逸的隐患。"},
    {"权杖五", "象征健康的竞争与能量的碰撞，代表通过冲突激发潜能与检验实力，蕴含竞争带来的成长与动力。", "代表冲突升级或内耗的状态，暗示无意义的争斗与自我消耗，显示竞争失控与团队协作的破裂。"},
    {"权杖六", "象征公众的认可与胜利的荣耀，代表努力获得回报与领导地位的确立，蕴含自信与成就感的提升。", "代表骄傲自满或胜利短暂的状态，暗示过度依赖外在认可与虚假的荣耀，显示成功根基不稳与傲慢。"},
    {"权杖七", "象征在压力下坚持立场与捍卫成果，代表面对挑战不退缩的勇气，蕴含逆境中坚守原则的韧性。", "代表放弃抵抗或不堪重负的状态，暗示缺乏自信与过度妥协，显示防线崩溃与轻易放弃立场的倾向。"},
    {"权杖八", "象征快速进展与信息的流通，代表事态的加速与机遇的迅速到来，蕴含行动果断与时机把握的智慧。", "代表延迟、混乱或方向错误的状态，暗示急躁导致的失误与沟通不畅，显示速度失控与计划被打乱。"},
    {"权杖九", "象征经历挑战后的坚韧与警觉，代表在疲惫中依然坚守的毅力，蕴含最后防线上的不屈与经验积累。", "代表偏执、防御过度或崩溃的状态，暗示因过往创伤而过度戒备，显示精力耗尽与无法放下的负担。"},
    {"权杖十", "象征承担过多责任与压力的极限，代表为达成目标而背负沉重负担，蕴含坚持到底的牺牲与承诺。", "代表不堪重负或拒绝承担的状态，暗示责任的崩溃与压力的爆发，显示过度承诺导致的身心俱疲。"},
    {"权杖侍从", "象征对新领域的热情探索与创意的萌发，代表充满好奇心的学习阶段，蕴含冒险精神与灵感的闪现。", "代表冲动行事或缺乏计划的状态，暗示热情有余而耐心不足，显示消息延迟或探索方向的错误。"},
    {"权杖骑士", "象征充满激情的冒险与果敢的行动，代表对目标的热情追求与无畏的前进，蕴含动能与勇气的结合。", "代表鲁莽冲动或缺乏耐性的状态，暗示行动未经思考与过度激进，显示热情失控与目标偏离的危险。"},
    {"权杖王后", "象征成熟的热情与独立的魅力，代表自信与创造力的完美结合，蕴含温暖而有力的领导与慷慨的分享。", "代表专横、嫉妒或情绪不稳的状态，暗示控制欲过强与自我中心，显示热情转化为压迫与信任的缺失。"},
    {"权杖国王", "象征成熟的领导力与清晰的远见，代表对事业的掌控与创业精神的实现，蕴含权威与责任的平衡。", "代表独裁、急躁或暴政的状态，暗示滥用权力与缺乏耐心，显示领导风格的压迫与决策的独断专行。"},
};

/* 权杖 14 张（英文） */
static const tarot_card_t s_tarot_wands_en[] = {
    {"Ace of Wands", "Symbolizes the initial spark of creativity and passion; it represents the launch of a brand-new plan of action, carrying the pure energy of the fire element and the drive to press forward courageously.", "Represents delayed action or blocked creativity; it suggests scattered energy and a lack of direction, revealing a beginning postponed or enthusiasm quickly fading."},
    {"Two of Wands", "Symbolizes planning for the future and the weighing of possibilities; it represents a choice between the known and the unknown, carrying foresight and strategic thinking.", "Represents fear of change or decision paralysis; it suggests excessive caution leading to missed opportunities, revealing planning that lacks the confidence and courage to act."},
    {"Three of Wands", "Symbolizes the emergence of initial results and the broadening of horizons; it represents progress brought by cooperation and opportunities from afar, carrying expectations of growth and exploration.", "Represents blocked cooperation or delayed plans; it suggests conflict within a team and diverging goals, revealing expansion meeting unexpected obstacles."},
    {"Four of Wands", "Symbolizes the establishment of a stable foundation and the celebration of achievement; it represents security and belonging in a harmonious environment, carrying community support and a staged victory.", "Represents instability or a lack of belonging; it suggests surface harmony concealing inner conflict, revealing an unstable foundation and the hidden danger of excessive comfort."},
    {"Five of Wands", "Symbolizes healthy competition and the clash of energies; it represents potential stimulated and strength tested through conflict, carrying the growth and motivation that competition brings.", "Represents escalating conflict or internal friction; it suggests meaningless strife and self-consumption, revealing competition spinning out of control and the breakdown of teamwork."},
    {"Six of Wands", "Symbolizes public recognition and the glory of victory; it represents effort rewarded and leadership established, carrying a rise in confidence and a sense of achievement.", "Represents complacency or short-lived victory; it suggests over-reliance on external approval and false glory, revealing an unstable foundation for success and arrogance."},
    {"Seven of Wands", "Symbolizes holding one's ground under pressure and defending what has been achieved; it represents the courage to face challenges without retreating, carrying the resilience to uphold principles in adversity.", "Represents giving up resistance or being overwhelmed; it suggests a lack of confidence and excessive compromise, revealing collapsing defenses and a tendency to abandon one's position too easily."},
    {"Eight of Wands", "Symbolizes rapid progress and the free flow of information; it represents the acceleration of events and the swift arrival of opportunities, carrying the wisdom of decisive action and seizing the moment.", "Represents delay, confusion, or a wrong direction; it suggests mistakes caused by haste and poor communication, revealing speed out of control and plans thrown into disarray."},
    {"Nine of Wands", "Symbolizes resilience and vigilance after enduring challenges; it represents the perseverance to hold on despite exhaustion, carrying unyielding strength at the last line of defense and accumulated experience.", "Represents paranoia, excessive defensiveness, or breakdown; it suggests over-vigilance due to past wounds, revealing depleted energy and burdens that cannot be laid down."},
    {"Ten of Wands", "Symbolizes taking on too many responsibilities and reaching the limit of pressure; it represents carrying a heavy burden to achieve a goal, carrying the sacrifice and commitment of persevering to the end.", "Represents being overwhelmed or refusing to take responsibility; it suggests the collapse of responsibility and an outburst of pressure, revealing physical and mental exhaustion caused by over-commitment."},
    {"Page of Wands", "Symbolizes enthusiastic exploration of new fields and the budding of ideas; it represents a learning stage full of curiosity, carrying a spirit of adventure and flashes of inspiration.", "Represents impulsive action or a lack of planning; it suggests more enthusiasm than patience, revealing delayed news or exploration in the wrong direction."},
    {"Knight of Wands", "Symbolizes passionate adventure and bold action; it represents the fervent pursuit of goals and fearless advance, carrying a union of momentum and courage.", "Represents recklessness or impatience; it suggests acting without thinking and excessive radicalism, revealing the danger of uncontrolled enthusiasm and deviation from the goal."},
    {"Queen of Wands", "Symbolizes mature passion and independent charisma; it represents the perfect union of confidence and creativity, carrying warm yet powerful leadership and generous sharing.", "Represents domineering behavior, jealousy, or emotional instability; it suggests an excessive desire for control and self-centeredness, revealing passion turning into oppression and a loss of trust."},
    {"King of Wands", "Symbolizes mature leadership and clear vision; it represents command over one's career and the realization of an entrepreneurial spirit, carrying a balance of authority and responsibility.", "Represents dictatorship, impatience, or tyranny; it suggests abuse of power and a lack of patience, revealing an oppressive leadership style and arbitrary, autocratic decision-making."},
};

/* 圣杯 14 张 */
static const tarot_card_t s_tarot_cups[] = {
    {"圣杯一", "象征情感源泉的涌现与爱的纯粹开端，代表心灵觉醒与直觉的开启，蕴含深情与接纳的温柔能量。", "代表情感封闭或爱的流失的状态，暗示内心空虚与直觉受阻，显示情感源泉的枯竭与自我隔绝。"},
    {"圣杯二", "象征和谐平等的关系与相互的联结，代表合作中的愉悦与情感的共鸣，蕴含信任与互惠的平衡。", "代表关系失衡或信任破裂的状态，暗示不平等的情感交换与合作的失败，显示联结中的裂痕与误解。"},
    {"圣杯三", "象征情感丰盛中的庆祝与真挚友谊，代表社群中的欢乐与共享的喜悦，蕴含人际联结中的温暖支持。", "代表表面的欢乐或过度社交的状态，暗示情感疏离与群体中的孤独，显示庆祝背后的空虚与过度放纵。"},
    {"圣杯四", "象征对现状的倦怠与内在反思，代表情感需求的重新审视与对更多可能性的静默等待，蕴含内省的契机。", "代表觉醒、接受或新机遇被注意的状态，暗示从倦怠中走出与重新投入，显示冷漠被融化与开放心态。"},
    {"圣杯五", "象征失落后的悲伤与必要的哀悼，代表情感创伤的面对与疗愈的开始，蕴含在痛苦中寻找意义的智慧。", "代表沉溺悲伤或拒绝放下的状态，暗示过度聚焦损失而忽视尚存的美好，显示疗愈过程被拖延与执念。"},
    {"圣杯六", "象征美好回忆与纯真年代的温暖，代表过去经验对当下的滋养与无害的怀旧，蕴含天真与保护的温柔。", "代表沉溺过去或拒绝成长的状态，暗示过度依赖回忆而逃避当下，显示纯真被扭曲与成长停滞的困境。"},
    {"圣杯七", "象征丰富的幻想与众多选择的呈现，代表欲望与可能性的交织，蕴含在迷宫中寻找真实需求的探索。", "代表幻想破灭或决策清晰的状态，暗示从迷雾中觉醒与选择的减少，显示逃避现实被直面与目标的明确。"},
    {"圣杯八", "象征主动离开以寻求更高满足，代表对现有情感状态的超越与不完美的接纳，蕴含放弃与重新出发的勇气。", "代表逃避问题或恐惧改变的状态，暗示在不满中徘徊与拒绝离开，显示寻求被延迟与改变的阻力。"},
    {"圣杯九", "象征情感满足与愿望实现的喜悦，代表内在丰盛与感恩的状态，蕴含知足常乐与情感上的圆满体验。", "代表贪婪、不满足或虚荣的状态，暗示对情感满足的过度追求与内在的空虚，显示丰盛表象下的匮乏。"},
    {"圣杯十", "象征家庭和谐与情感关系的极致圆满，代表灵魂层面的联结与持久的幸福，蕴含爱与和平的至高境界。", "代表家庭冲突或情感圆满受阻的状态，暗示和谐表面的破裂与理想化的幻灭，显示关系中的深层裂痕。"},
    {"圣杯侍从", "象征直觉的觉醒与情感消息的到来，代表对内心世界的敏感探索与创意的萌芽，蕴含温柔与想象的力量。", "代表情感消息延迟或过度敏感的状态，暗示情绪化的判断与逃避现实，显示直觉被焦虑扭曲与情感不成熟。"},
    {"圣杯骑士", "象征浪漫情怀与理想追求，代表情感上的冒险与对美好关系的向往，蕴含温柔行动与诗意表达的魅力。", "代表承诺恐惧或情感逃避的状态，暗示理想化与现实的脱节，显示浪漫表象下的不稳定与缺乏担当。"},
    {"圣杯王后", "象征深层的同理心与无条件的滋养，代表情感成熟的巅峰与直觉的精准，蕴含温柔包容与心灵疗愈的力量。", "代表过度情绪化或依赖的状态，暗示同理心被滥用与情感操控，显示滋养转化为溺爱与边界模糊。"},
    {"圣杯国王", "象征对情感的成熟掌控与智慧的慈悲，代表以理性引导情感与以关怀领导他人，蕴含平衡与深度的结合。", "代表情感压抑或冷漠操纵的状态，暗示以理性为名的情感隔离与伪装的关怀，显示内在情感的暗涌与疏离。"},
};

/* 圣杯 14 张（英文） */
static const tarot_card_t s_tarot_cups_en[] = {
    {"Ace of Cups", "Symbolizes the welling-up of the emotional source and the pure beginning of love; it represents spiritual awakening and the opening of intuition, carrying the gentle energy of deep affection and acceptance.", "Represents emotional closure or the draining away of love; it suggests inner emptiness and blocked intuition, revealing the drying-up of the emotional source and self-isolation."},
    {"Two of Cups", "Symbolizes a harmonious and equal relationship with mutual connection; it represents joy in cooperation and emotional resonance, carrying a balance of trust and reciprocity.", "Represents an imbalanced relationship or broken trust; it suggests unequal emotional exchange and failed cooperation, revealing cracks and misunderstandings in the connection."},
    {"Three of Cups", "Symbolizes celebration amid emotional abundance and sincere friendship; it represents joy within a community and happiness that is shared, carrying warm support in human connections.", "Represents superficial merriment or excessive socializing; it suggests emotional estrangement and loneliness within a group, revealing emptiness behind the celebration and over-indulgence."},
    {"Four of Cups", "Symbolizes weariness with the status quo and inner reflection; it represents a re-examination of emotional needs and a quiet waiting for further possibilities, carrying an opportunity for introspection.", "Represents awakening, acceptance, or new opportunities being noticed; it suggests emerging from weariness and re-engaging with life, revealing coldness melting away and an opening of the heart."},
    {"Five of Cups", "Symbolizes grief after loss and necessary mourning; it represents facing emotional wounds and the beginning of healing, carrying the wisdom of finding meaning within pain.", "Represents wallowing in grief or refusing to let go; it suggests focusing excessively on loss while overlooking the good that remains, revealing a healing process delayed by attachment."},
    {"Six of Cups", "Symbolizes fond memories and the warmth of innocent years; it represents how past experience nourishes the present, and harmless nostalgia, carrying the tenderness of innocence and protection.", "Represents dwelling in the past or refusing to grow; it suggests over-reliance on memories as an escape from the present, revealing innocence distorted and growth stalled."},
    {"Seven of Cups", "Symbolizes rich fantasy and the presentation of many choices; it represents the interweaving of desires and possibilities, carrying an exploration through the maze in search of one's true needs.", "Represents shattered illusions or clarity in decision-making; it suggests awakening from the mist and a narrowing of choices, revealing escapism confronted and goals made clear."},
    {"Eight of Cups", "Symbolizes voluntarily leaving in search of higher fulfillment; it represents transcending one's current emotional state and accepting imperfection, carrying the courage to give up and set out anew.", "Represents avoiding problems or fearing change; it suggests lingering in dissatisfaction and refusing to leave, revealing the search postponed and resistance to change."},
    {"Nine of Cups", "Symbolizes emotional satisfaction and the joy of wishes fulfilled; it represents inner abundance and gratitude, carrying contentment and a consummate emotional experience.", "Represents greed, dissatisfaction, or vanity; it suggests an excessive pursuit of emotional gratification and inner emptiness, revealing scarcity beneath an appearance of abundance."},
    {"Ten of Cups", "Symbolizes family harmony and the ultimate fulfillment of emotional relationships; it represents soul-level connection and lasting happiness, carrying the supreme state of love and peace.", "Represents family conflict or blocked emotional fulfillment; it suggests the breaking of a harmonious surface and the disillusionment of idealization, revealing deep cracks in relationships."},
    {"Page of Cups", "Symbolizes the awakening of intuition and the arrival of emotional news; it represents sensitive exploration of the inner world and the budding of creativity, carrying the power of gentleness and imagination.", "Represents delayed emotional news or oversensitivity; it suggests emotional judgment and escape from reality, revealing intuition distorted by anxiety and emotional immaturity."},
    {"Knight of Cups", "Symbolizes romantic sentiment and the pursuit of ideals; it represents emotional adventure and the yearning for a beautiful relationship, carrying the charm of gentle action and poetic expression.", "Represents fear of commitment or emotional avoidance; it suggests idealization disconnected from reality, revealing instability beneath a romantic surface and a lack of responsibility."},
    {"Queen of Cups", "Symbolizes deep empathy and unconditional nurturing; it represents the peak of emotional maturity and precise intuition, carrying the power of gentle acceptance and spiritual healing.", "Represents excessive emotionality or dependence; it suggests empathy misused and emotional manipulation, revealing nurturing turned into spoiling and blurred boundaries."},
    {"King of Cups", "Symbolizes mature command of emotions and wise compassion; it represents guiding emotions with reason and leading others with care, carrying a union of balance and depth.", "Represents emotional suppression or cold manipulation; it suggests emotional isolation in the name of reason and feigned concern, revealing undercurrents of feeling and estrangement within."},
};

/* 宝剑 14 张 */
static const tarot_card_t s_tarot_swords[] = {
    {"宝剑一", "象征思想力量的觉醒与真理的锐利揭示，代表清晰洞察与果断决策的开端，蕴含理智与正义的纯粹能量。", "代表混乱思维或错误判断的状态，暗示真理被扭曲与决策的失误，显示心智开端受阻与沟通中的误解。"},
    {"宝剑二", "象征两难中的僵局与信息的回避，代表在矛盾中保持平衡以等待时机，蕴含以静制动与避免仓促的智慧。", "代表真相揭露或僵局打破的状态，暗示信息涌现与决策的被迫进行，显示回避不再可行与选择的紧迫。"},
    {"宝剑三", "象征深刻的心痛与情感创伤的显现，代表悲伤的必然性与接受的开始，蕴含在痛苦中寻找成长契机的智慧。", "代表从悲伤中恢复或逃避痛苦的状态，暗示旧伤复发与压抑的释放，显示疗愈进程中的反复与情绪波动。"},
    {"宝剑四", "象征从冲突中退隐以恢复心智，代表必要的休息与内在平静的重建，蕴含暂停以积蓄力量的智慧。", "代表焦躁不安或逃避行动的状态，暗示过度休息导致的停滞与错失恢复后的行动时机，显示惰性。"},
    {"宝剑五", "象征冲突中的胜利与策略的成功，代表以智取胜但可能伴随代价，蕴含竞争中的冷酷与必要的果断。", "代表冲突的延续或和解的可能，暗示胜利的空虚与关系的修复，显示对抗的无意义与和解的契机。"},
    {"宝剑六", "象征离开困境向平静过渡的旅程，代表从混乱走向秩序的疗愈过程，蕴含放下过去以迎接宁静的智慧。", "代表过渡受阻或拒绝离开的状态，暗示困于过去与重复同样的错误，显示疗愈旅程的停滞与方向的迷失。"},
    {"宝剑七", "象征策略性的思考与隐秘的行动，代表以非常规手段解决问题的智慧，蕴含机智与变通但伴随道德考验。", "代表策略失败或欺骗被揭穿的状态，暗示计划漏洞与信任危机，显示非常规手段的反噬与诚信的丧失。"},
    {"宝剑八", "象征自我设限的束缚与无力感的困境，代表被负面思维困住而失去行动自由，蕴含认知牢笼的隐喻。", "代表解放或自我设限解除的状态，暗示新的视角与勇气的恢复，显示束缚被意识到并逐渐松动的契机。"},
    {"宝剑九", "象征深夜的焦虑与心智的煎熬，代表恐惧被放大与负面思维的循环，蕴含面对内心阴影的必要性。", "代表焦虑减轻或恐惧被面对的状态，暗示希望的出现与负面循环的打破，显示煎熬的缓解与心灵的释放。"},
    {"宝剑十", "象征痛苦周期的顶点与旧有模式的终结，代表最黑暗时刻的降临与必然结束，蕴含毁灭后重生的极端能量。", "代表毁灭后的残余或无法结束的状态，暗示在痛苦边缘徘徊与对终结的恐惧，显示结束被延迟与持续的折磨。"},
    {"宝剑侍从", "象征对新思想的好奇与敏锐的观察，代表学习阶段的开启与信息的收集，蕴含求知欲与沟通技巧的精进。", "代表八卦、谣言或缺乏深度的状态，暗示信息过载与肤浅的判断，显示好奇心被误用与沟通的失当。"},
    {"宝剑骑士", "象征果断行动与激进的思想冲锋，代表以迅速决策直面挑战的勇气，蕴含智力与行动力的高度结合。", "代表鲁莽、暴力或缺乏思考的状态，暗示行动过于急躁与言语的伤人，显示冲锋失控与冲突的升级。"},
    {"宝剑王后", "象征思想的独立与判断的清晰，代表以理性直面真相而不被情感左右，蕴含公正、客观与高标准的力量。", "代表冷酷、批判或过度理性的状态，暗示以理智为名的情感伤害与标准的严苛，显示独立演变为孤立。"},
    {"宝剑国王", "象征思想权威的建立与真理的坚定捍卫，代表以智慧领导与以逻辑决策，蕴含心智成熟与公正裁决的力量。", "代表专断、滥用权力或冷酷无情的状态，暗示以真理为名的压迫与情感的彻底否定，显示权威沦为暴政。"},
};

/* 宝剑 14 张（英文） */
static const tarot_card_t s_tarot_swords_en[] = {
    {"Ace of Swords", "Symbolizes the awakening of mental power and the sharp revelation of truth; it represents the beginning of clear insight and decisive judgment, carrying the pure energy of reason and justice.", "Represents confused thinking or erroneous judgment; it suggests truth distorted and decisions mishandled, revealing a blocked mental beginning and misunderstandings in communication."},
    {"Two of Swords", "Symbolizes a stalemate between two difficult options and the avoidance of information; it represents maintaining balance amid contradiction while waiting for the right moment, carrying the wisdom of stillness overcoming motion and avoiding haste.", "Represents truth revealed or a stalemate broken; it suggests an influx of information and a decision being forced, revealing that avoidance is no longer viable and a choice is urgent."},
    {"Three of Swords", "Symbolizes deep heartache and the surfacing of emotional wounds; it represents the inevitability of sorrow and the beginning of acceptance, carrying the wisdom of finding opportunities for growth within pain.", "Represents recovery from grief or avoidance of pain; it suggests old wounds reopening and the release of suppression, revealing relapses and emotional fluctuations in the healing process."},
    {"Four of Swords", "Symbolizes retreating from conflict to restore the mind; it represents necessary rest and the rebuilding of inner peace, carrying the wisdom of pausing to gather strength.", "Represents restlessness or avoidance of action; it suggests stagnation caused by excessive rest and missing the moment to act after recovery, revealing inertia."},
    {"Five of Swords", "Symbolizes victory in conflict and strategic success; it represents winning by wit, possibly at a cost, carrying the coldness of competition and necessary decisiveness.", "Represents the continuation of conflict or the possibility of reconciliation; it suggests the emptiness of victory and the mending of relationships, revealing the meaninglessness of confrontation and an opportunity for peace."},
    {"Six of Swords", "Symbolizes a journey of transition away from difficulty toward calm; it represents a healing process from chaos to order, carrying the wisdom of leaving the past behind to embrace tranquility.", "Represents a blocked transition or a refusal to leave; it suggests being trapped in the past and repeating the same mistakes, revealing stagnation in the healing journey and a loss of direction."},
    {"Seven of Swords", "Symbolizes strategic thinking and covert action; it represents the wisdom of solving problems by unconventional means, carrying wit and flexibility accompanied by a moral test.", "Represents a failed strategy or exposed deception; it suggests flaws in the plan and a crisis of trust, revealing unconventional methods backfiring and integrity lost."},
    {"Eight of Swords", "Symbolizes self-imposed bondage and a predicament of powerlessness; it represents being trapped by negative thinking and losing freedom of action, carrying the metaphor of a cognitive cage.", "Represents liberation or the lifting of self-imposed limits; it suggests a new perspective and the return of courage, revealing an opportunity in which the bondage is recognized and gradually loosened."},
    {"Nine of Swords", "Symbolizes late-night anxiety and mental torment; it represents fear magnified and a cycle of negative thinking, carrying the necessity of facing one's inner shadows.", "Represents easing anxiety or fear being confronted; it suggests the appearance of hope and the breaking of the negative cycle, revealing relief from torment and release of the mind."},
    {"Ten of Swords", "Symbolizes the peak of a painful cycle and the end of an old pattern; it represents the arrival of the darkest moment and its inevitable conclusion, carrying the extreme energy of rebirth after ruin.", "Represents remnants after ruin or a state that cannot end; it suggests hovering at the edge of pain and fearing the end, revealing a conclusion delayed and torment prolonged."},
    {"Page of Swords", "Symbolizes curiosity about new ideas and keen observation; it represents the opening of a learning stage and the gathering of information, carrying a thirst for knowledge and the refinement of communication skills.", "Represents gossip, rumors, or a lack of depth; it suggests information overload and superficial judgment, revealing curiosity misused and communication mishandled."},
    {"Knight of Swords", "Symbolizes decisive action and a radical charge of thought; it represents the courage to face challenges with swift decisions, carrying a high union of intellect and action.", "Represents recklessness, violence, or a lack of thought; it suggests acting too hastily and words that wound, revealing a charge out of control and conflict escalating."},
    {"Queen of Swords", "Symbolizes independence of thought and clarity of judgment; it represents facing truth with reason without being swayed by emotion, carrying the power of fairness, objectivity, and high standards.", "Represents coldness, harsh criticism, or excessive rationality; it suggests emotional harm done in the name of reason and overly severe standards, revealing independence turning into isolation."},
    {"King of Swords", "Symbolizes the establishment of intellectual authority and the firm defense of truth; it represents leading with wisdom and deciding with logic, carrying the power of mental maturity and fair judgment.", "Represents arbitrariness, abuse of power, or cold-bloodedness; it suggests oppression in the name of truth and the total denial of emotion, revealing authority degraded into tyranny."},
};

/* 星币 14 张 */
static const tarot_card_t s_tarot_pents[] = {
    {"星币一", "象征物质机遇的降临与财富的初始萌芽，代表现实世界中新计划的可行性，蕴含土元素的稳定与丰饶潜力。", "代表错失机遇或物质计划受阻的状态，暗示资源浪费与根基不稳，显示开端缺乏实际支撑与过度理想化。"},
    {"星币二", "象征资源调配中的平衡艺术与灵活变通，代表在多任务中维持稳定的能力，蕴含从容应对与管理的智慧。", "代表失衡、过度扩展或财务混乱的状态，暗示资源分配失当与精力分散，显示变通失效与管理的失控。"},
    {"星币三", "象征团队合作与技艺的精进，代表工作项目中的初步成果与专业认可，蕴含协作中的学习与技能的提升。", "代表合作不畅或技能不足的状态，暗示团队沟通障碍与质量标准的下滑，显示进展受阻与专业精神的缺失。"},
    {"星币四", "象征对物质资源的保守掌控与财务的稳固，代表安全感的建立与资源的积累，蕴含谨慎理财与边界维护。", "代表贪婪、吝啬或物质失控的状态，暗示对财富的过度执念与分享的拒绝，显示固守演变为孤立与停滞。"},
    {"星币五", "象征物质困境与贫困的考验，代表财务损失或健康挑战带来的艰难时期，蕴含在逆境中寻求帮助的谦卑。", "代表困境缓解或拒绝帮助的状态，暗示恢复期开始与援助的到来，显示孤立无援被突破与重建的希望。"},
    {"星币六", "象征慷慨的给予与公平的资源交换，代表财务平衡与互助精神，蕴含施与受中的尊严与良性循环。", "代表债务、依赖或不公平交换的状态，暗示慷慨被利用与条件附加的操控，显示流通受阻与关系的失衡。"},
    {"星币七", "象征对投资成果的耐心评估与等待收获，代表长期规划中的阶段性审视，蕴含耕耘后期待回报的成熟心态。", "代表焦虑、不耐烦或投资失败的状态，暗示对成果的过度急切与计划的偏差，显示评估中的焦虑与收获延迟。"},
    {"星币八", "象征对技艺的专注打磨与工作的精益求精，代表专业能力的提升与细节的关注，蕴含勤奋与工匠精神的荣耀。", "代表缺乏动力或完美主义失控的状态，暗示敷衍了事与过度纠结细节，显示精进受阻与职业倦怠。"},
    {"星币九", "象征物质独立与感官享受的达成，代表自给自足与优雅生活的实现，蕴含努力后的安逸与自我奖赏的尊严。", "代表过度依赖或虚荣空虚的状态，暗示物质享受的成瘾与内在价值的忽视，显示独立表象下的不安全感。"},
    {"星币十", "象征家族财富的积累与物质传承的圆满，代表长期投资的回报与世代安稳，蕴含物质层面的最高成就。", "代表财务损失或家族纷争的状态，暗示传承中断与物质基础的动摇，显示圆满表象下的裂痕与不稳定。"},
    {"星币侍从", "象征务实的学习态度与物质计划的开端，代表对现实目标的认真追求与技能的扎实积累，蕴含勤奋与专注。", "代表缺乏计划或好高骛远的状态，暗示对物质目标的空想与执行的懒惰，显示开端浮躁与基础不牢。"},
    {"星币骑士", "象征勤奋踏实的行动与可靠的承诺，代表对物质目标的稳健追求与责任的承担，蕴含耐心与坚持不懈的美德。", "代表固执、懒惰或过度保守的状态，暗示缺乏弹性与拒绝改变，显示稳健演变为停滞与错失变革的机遇。"},
    {"星币王后", "象征对物质世界的滋养与丰饶的创造，代表务实中的温暖与对自然的亲近，蕴含慷慨与务实的完美结合。", "代表过度依赖物质或缺乏安全感的操控，暗示对资源的控制与情感的物化，显示滋养转化为占有与焦虑。"},
    {"星币国王", "象征物质成就的巅峰与务实权威的建立，代表对财富与资源的成熟掌控，蕴含商业智慧与长远规划的领导力。", "代表物质主义、贪婪或固执的状态，暗示对财富的执念与情感的冷漠，显示成就背后的孤立与价值的扭曲。"},
};

/* 星币 14 张（英文） */
static const tarot_card_t s_tarot_pents_en[] = {
    {"Ace of Pentacles", "Symbolizes the arrival of material opportunity and the first sprouting of wealth; it represents the feasibility of a new plan in the real world, carrying the earth element's stability and potential for abundance.", "Represents missed opportunities or blocked material plans; it suggests wasted resources and an unstable foundation, revealing a beginning lacking practical support and excessive idealism."},
    {"Two of Pentacles", "Symbolizes the art of balancing resources and flexible adaptation; it represents the ability to maintain stability amid multiple tasks, carrying the wisdom of composed response and management.", "Represents imbalance, overextension, or financial chaos; it suggests misallocated resources and scattered energy, revealing adaptability failing and management spinning out of control."},
    {"Three of Pentacles", "Symbolizes teamwork and the refinement of craft; it represents initial results in a work project and professional recognition, carrying learning through collaboration and the improvement of skills.", "Represents poor cooperation or insufficient skill; it suggests communication barriers within a team and declining quality standards, revealing hindered progress and a lack of professionalism."},
    {"Four of Pentacles", "Symbolizes conservative control of material resources and financial stability; it represents the building of security and the accumulation of resources, carrying prudent money management and the maintenance of boundaries.", "Represents greed, miserliness, or material loss of control; it suggests excessive attachment to wealth and a refusal to share, revealing holding fast turning into isolation and stagnation."},
    {"Five of Pentacles", "Symbolizes material hardship and the trial of poverty; it represents difficult times brought by financial loss or health challenges, carrying the humility to seek help in adversity.", "Represents easing hardship or refusing help; it suggests the beginning of recovery and the arrival of assistance, revealing isolation being broken through and the hope of rebuilding."},
    {"Six of Pentacles", "Symbolizes generous giving and the fair exchange of resources; it represents financial balance and a spirit of mutual aid, carrying dignity in giving and receiving and a virtuous cycle.", "Represents debt, dependence, or unfair exchange; it suggests generosity being exploited and manipulation with strings attached, revealing blocked circulation and imbalanced relationships."},
    {"Seven of Pentacles", "Symbolizes patient assessment of investment results and waiting for the harvest; it represents a periodic review within long-term planning, carrying the mature mindset of expecting returns after cultivation.", "Represents anxiety, impatience, or failed investment; it suggests excessive eagerness for results and deviations in the plan, revealing anxiety in assessment and a delayed harvest."},
    {"Eight of Pentacles", "Symbolizes focused honing of craft and striving for excellence in work; it represents the improvement of professional ability and attention to detail, carrying the honor of diligence and the artisan spirit.", "Represents a lack of motivation or perfectionism out of control; it suggests perfunctory work and excessive fixation on details, revealing improvement hindered and professional burnout."},
    {"Nine of Pentacles", "Symbolizes material independence and the attainment of sensory enjoyment; it represents self-sufficiency and the realization of an elegant life, carrying well-earned comfort and the dignity of rewarding oneself.", "Represents over-dependence or vain emptiness; it suggests addiction to material pleasure and the neglect of inner values, revealing insecurity beneath an appearance of independence."},
    {"Ten of Pentacles", "Symbolizes the accumulation of family wealth and the fulfillment of material legacy; it represents returns on long-term investment and security across generations, carrying the highest achievement on the material plane.", "Represents financial loss or family disputes; it suggests an interrupted legacy and a shaken material foundation, revealing cracks and instability beneath an appearance of fulfillment."},
    {"Page of Pentacles", "Symbolizes a pragmatic attitude toward learning and the beginning of a material plan; it represents the earnest pursuit of realistic goals and the solid accumulation of skills, carrying diligence and focus.", "Represents a lack of planning or overambitious dreaming; it suggests fantasizing about material goals and laziness in execution, revealing a restless beginning and an unstable foundation."},
    {"Knight of Pentacles", "Symbolizes diligent and steady action with reliable commitment; it represents the steady pursuit of material goals and the acceptance of responsibility, carrying the virtues of patience and perseverance.", "Represents stubbornness, laziness, or excessive conservatism; it suggests inflexibility and a refusal to change, revealing steadiness turning into stagnation and missed opportunities for transformation."},
    {"Queen of Pentacles", "Symbolizes nurturing of the material world and the creation of abundance; it represents warmth within practicality and closeness to nature, carrying a perfect union of generosity and pragmatism.", "Represents over-reliance on material things or manipulation born of insecurity; it suggests control over resources and the materialization of emotions, revealing nurturing turned into possessiveness and anxiety."},
    {"King of Pentacles", "Symbolizes the peak of material achievement and the establishment of pragmatic authority; it represents mature command of wealth and resources, carrying business acumen and the leadership of long-term planning.", "Represents materialism, greed, or obstinacy; it suggests fixation on wealth and emotional coldness, revealing isolation behind achievement and distorted values."},
};

#define TAROT_MAJOR_COUNT (sizeof(s_tarot_major_arcana)/sizeof(s_tarot_major_arcana[0]))
#define TAROT_WANDS_COUNT (sizeof(s_tarot_wands)/sizeof(s_tarot_wands[0]))
#define TAROT_CUPS_COUNT (sizeof(s_tarot_cups)/sizeof(s_tarot_cups[0]))
#define TAROT_SWORDS_COUNT (sizeof(s_tarot_swords)/sizeof(s_tarot_swords[0]))
#define TAROT_PENTS_COUNT (sizeof(s_tarot_pents)/sizeof(s_tarot_pents[0]))
#define TAROT_TOTAL_COUNT (TAROT_MAJOR_COUNT + TAROT_WANDS_COUNT + TAROT_CUPS_COUNT + TAROT_SWORDS_COUNT + TAROT_PENTS_COUNT)

/**
 * @brief 根据索引获取塔罗牌数据
 */
static inline const tarot_card_t* tarot_get_card_by_index(int idx)
{
    if (idx < 0 || idx >= TAROT_TOTAL_COUNT) {
        return NULL;
    }
    if (idx < TAROT_MAJOR_COUNT) {
        return &s_tarot_major_arcana[idx];
    }
    idx -= TAROT_MAJOR_COUNT;
    if (idx < TAROT_WANDS_COUNT) {
        return &s_tarot_wands[idx];
    }
    idx -= TAROT_WANDS_COUNT;
    if (idx < TAROT_CUPS_COUNT) {
        return &s_tarot_cups[idx];
    }
    idx -= TAROT_CUPS_COUNT;
    if (idx < TAROT_SWORDS_COUNT) {
        return &s_tarot_swords[idx];
    }
    idx -= TAROT_SWORDS_COUNT;
    if (idx < TAROT_PENTS_COUNT) {
        return &s_tarot_pents[idx];
    }
    return NULL;
}

/**
 * @brief 根据索引获取塔罗牌英文数据
 */
static inline const tarot_card_t* tarot_get_card_by_index_en(int idx)
{
    if (idx < 0 || idx >= TAROT_TOTAL_COUNT) {
        return NULL;
    }
    if (idx < TAROT_MAJOR_COUNT) {
        return &s_tarot_major_arcana_en[idx];
    }
    idx -= TAROT_MAJOR_COUNT;
    if (idx < TAROT_WANDS_COUNT) {
        return &s_tarot_wands_en[idx];
    }
    idx -= TAROT_WANDS_COUNT;
    if (idx < TAROT_CUPS_COUNT) {
        return &s_tarot_cups_en[idx];
    }
    idx -= TAROT_CUPS_COUNT;
    if (idx < TAROT_SWORDS_COUNT) {
        return &s_tarot_swords_en[idx];
    }
    idx -= TAROT_SWORDS_COUNT;
    if (idx < TAROT_PENTS_COUNT) {
        return &s_tarot_pents_en[idx];
    }
    return NULL;
}

#endif /* TAROT_H */
