#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
 
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/device.h>
#include <sound/control.h>
#include "common.h"

extern int snd_stm_debug_level;

struct snd_stm_conv_group;
struct snd_stm_conv_source;

struct snd_stm_conv_converter {
	struct list_head list;

	struct snd_stm_conv_group *group;

	struct snd_stm_conv_ops *ops;
	void *priv;

	int source_channel_from;
	int source_channel_to;

	int enabled;
	int muted_by_source;
	int muted_by_user;
	spinlock_t status_lock;  
	struct snd_kcontrol *ctl_mute;

	snd_stm_magic_field;
};

struct snd_stm_conv_group {
	struct list_head list;

	struct snd_stm_conv_source *source;

	struct list_head converters;

	int enabled;
	int muted_by_source;

	snd_stm_magic_field;

	char name[1];  
};

struct snd_stm_conv_source {
	struct list_head list;

	struct bus_type *bus;
	struct device_node *bus_id;
	int channels_num;
	struct snd_card *card;
	int card_device;

	struct snd_stm_conv_group *group_selected;
	struct snd_stm_conv_group *group_active;
	struct list_head groups;
	struct snd_kcontrol *ctl_route;

	snd_stm_magic_field;
};

static LIST_HEAD(snd_stm_conv_sources);  
#ifdef MY_DEF_HERE
spinlock_t snd_stm_conv_lock;
#else  
static DEFINE_MUTEX(snd_stm_conv_mutex);  
#endif  

const char *snd_stm_conv_get_name(struct snd_stm_conv_group *group)
{
	snd_stm_printd(1, "snd_stm_conv_get_name(group=%p)\n", group);

	BUG_ON(!group);
	BUG_ON(!snd_stm_magic_valid(group));

	return group->name;
}

unsigned int snd_stm_conv_get_format(struct snd_stm_conv_group *group)
{
	unsigned int result = 0;
	struct snd_stm_conv_converter *converter;

	snd_stm_printd(1, "snd_stm_conv_get_format(group=%p)\n", group);

	BUG_ON(!group);
	BUG_ON(!snd_stm_magic_valid(group));

	list_for_each_entry(converter, &group->converters, list) {
		unsigned int format = converter->ops->get_format(
				converter->priv);

		if (result == 0) {
			result = format;
		} else if (format != result) {
			snd_stm_printe("Wrong format (0x%x) for converter %p"
					" (the rest says 0x%x)!\n", format,
					converter, result);
			snd_BUG();
		}
	}

	return result;
}
EXPORT_SYMBOL(snd_stm_conv_get_format);

int snd_stm_conv_get_oversampling(struct snd_stm_conv_group *group)
{
	int result = 0;
	struct snd_stm_conv_converter *converter;

	snd_stm_printd(1, "snd_stm_conv_get_oversampling(group=%p)\n",
			group);

	BUG_ON(!group);
	BUG_ON(!snd_stm_magic_valid(group));

	list_for_each_entry(converter, &group->converters, list) {
		int oversampling;

		BUG_ON(!converter);
		BUG_ON(!snd_stm_magic_valid(converter));

		oversampling = converter->ops->get_oversampling(
				converter->priv);

		if (result == 0) {
			result = oversampling;
		} else if (oversampling != result) {
			snd_stm_printe("Wrong oversampling value (%d) for "
					"converter %p (the rest says %d)!\n",
					oversampling, converter, result);
			snd_BUG();
		}
	}

	return result;
}
EXPORT_SYMBOL(snd_stm_conv_get_oversampling);

int snd_stm_conv_enable(struct snd_stm_conv_group *group,
		int channel_from, int channel_to)
{
	int result = 0;
	struct snd_stm_conv_converter *converter;

	snd_stm_printd(1, "snd_stm_conv_enable(group=%p, channel_from=%d, "
			"channel_to=%d)\n", group, channel_from, channel_to);

	BUG_ON(channel_to < channel_from);
	BUG_ON(!group);
	BUG_ON(!snd_stm_magic_valid(group));
	BUG_ON(group->enabled);

	group->enabled = 1;

	list_for_each_entry(converter, &group->converters, list) {
		BUG_ON(!converter);
		BUG_ON(!snd_stm_magic_valid(converter));
		BUG_ON(converter->enabled);

		spin_lock(&converter->status_lock);

		if ((channel_from <= converter->source_channel_from &&
				converter->source_channel_from <= channel_to) ||
				(channel_from <= converter->source_channel_to &&
				converter->source_channel_to <= channel_to)) {
			converter->enabled = 1;

			if (converter->ops->set_enabled) {
				int done = converter->ops->set_enabled(1,
						converter->priv);

				if (done != 0) {
					snd_stm_printe("Failed to enable "
							"converter %p!\n",
							converter);
					result = done;
				}
			}
		}

		spin_unlock(&converter->status_lock);
	}

	return result;
}
EXPORT_SYMBOL(snd_stm_conv_enable);

int snd_stm_conv_disable(struct snd_stm_conv_group *group)
{
	int result = 0;
	struct snd_stm_conv_converter *converter;

	snd_stm_printd(1, "snd_stm_conv_disable(group=%p)\n", group);

	BUG_ON(!group);
	BUG_ON(!snd_stm_magic_valid(group));
	BUG_ON(!group->enabled);

	group->enabled = 0;

	list_for_each_entry(converter, &group->converters, list) {
		BUG_ON(!converter);
		BUG_ON(!snd_stm_magic_valid(converter));

		spin_lock(&converter->status_lock);

		if (converter->enabled) {
			converter->enabled = 0;

			if (converter->ops->set_enabled) {
				int done = converter->ops->set_enabled(0,
						converter->priv);

				if (done != 0) {
					snd_stm_printe("Failed to disable "
							"converter %p!\n",
							converter);
					result = done;
				}
			}
		}

		spin_unlock(&converter->status_lock);
	}

	return result;
}
EXPORT_SYMBOL(snd_stm_conv_disable);

int snd_stm_conv_mute(struct snd_stm_conv_group *group)
{
	int result = 0;
	struct snd_stm_conv_converter *converter;

	snd_stm_printd(1, "snd_stm_conv_mute(group=%p)\n", group);

	BUG_ON(!group);
	BUG_ON(!snd_stm_magic_valid(group));
	BUG_ON(!group->enabled);
	BUG_ON(group->muted_by_source);

	group->muted_by_source = 1;

	list_for_each_entry(converter, &group->converters, list) {
		BUG_ON(!converter);
		BUG_ON(!snd_stm_magic_valid(converter));

		spin_lock(&converter->status_lock);

		if (converter->enabled) {
			converter->muted_by_source = 1;

			if (converter->ops->set_muted &&
					!converter->muted_by_user) {
				int done = converter->ops->set_muted(1,
						converter->priv);

				if (done != 0) {
					snd_stm_printe("Failed to mute "
							"converter %p!\n",
							converter);
					result = done;
				}
			}
		}

		spin_unlock(&converter->status_lock);
	}

	return result;
}
EXPORT_SYMBOL(snd_stm_conv_mute);

int snd_stm_conv_unmute(struct snd_stm_conv_group *group)
{
	int result = 0;
	struct snd_stm_conv_converter *converter;

	snd_stm_printd(1, "snd_stm_conv_unmute(group=%p)\n", group);

	BUG_ON(!group);
	BUG_ON(!snd_stm_magic_valid(group));
	BUG_ON(!group->enabled);
	BUG_ON(!group->muted_by_source);

	group->muted_by_source = 0;

	list_for_each_entry(converter, &group->converters, list) {
		BUG_ON(!converter);
		BUG_ON(!snd_stm_magic_valid(converter));

		spin_lock(&converter->status_lock);

		if (converter->enabled) {
			converter->muted_by_source = 0;

			if (converter->ops->set_muted &&
					!converter->muted_by_user) {
				int done = converter->ops->set_muted(0,
						converter->priv);

				if (done != 0) {
					snd_stm_printe("Failed to unmute "
							"converter %p!\n",
							converter);
					result = done;
				}
			}
		}

		spin_unlock(&converter->status_lock);
	}

	return result;
}
EXPORT_SYMBOL(snd_stm_conv_unmute);

static int snd_stm_conv_ctl_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_conv_converter *converter = snd_kcontrol_chip(kcontrol);

	snd_stm_printd(1, "snd_stm_conv_ctl_mute_get(kcontrol=0x%p,"
			" ucontrol=0x%p)\n", kcontrol, ucontrol);

	BUG_ON(!converter);
	BUG_ON(!snd_stm_magic_valid(converter));

	spin_lock(&converter->status_lock);

	ucontrol->value.integer.value[0] = !converter->muted_by_user;

	spin_unlock(&converter->status_lock);

	return 0;
}

static int snd_stm_conv_ctl_mute_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_conv_converter *converter = snd_kcontrol_chip(kcontrol);
	int changed = 0;

	snd_stm_printd(1, "snd_stm_conv_ctl_mute_put(kcontrol=0x%p,"
			" ucontrol=0x%p)\n", kcontrol, ucontrol);

	BUG_ON(!converter);
	BUG_ON(!snd_stm_magic_valid(converter));
	BUG_ON(!converter->ops->set_muted);

	spin_lock(&converter->status_lock);

	if (ucontrol->value.integer.value[0] !=
			!converter->muted_by_user) {
		changed = 1;

		converter->muted_by_user =
				!ucontrol->value.integer.value[0];

		if (converter->enabled &&
				converter->muted_by_user &&
				!converter->muted_by_source)
			converter->ops->set_muted(1, converter->priv);
		else if (converter->enabled &&
				!converter->muted_by_user &&
				!converter->muted_by_source)
			converter->ops->set_muted(0, converter->priv);
	}

	spin_unlock(&converter->status_lock);

	return changed;
}

static struct snd_kcontrol_new snd_stm_conv_ctl_mute = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Master Playback Switch",
	.info = snd_stm_ctl_boolean_info,
	.get = snd_stm_conv_ctl_mute_get,
	.put = snd_stm_conv_ctl_mute_put,
};

static int snd_stm_conv_ctl_mute_add(struct snd_stm_conv_converter *converter)
{
	int result;
	struct snd_stm_conv_source *source;
	struct snd_kcontrol *ctl_mute;

	snd_stm_printd(1, "snd_stm_conv_ctl_mute_add(converter=%p)\n",
			converter);

	BUG_ON(!converter);
	BUG_ON(!snd_stm_magic_valid(converter));

	source = converter->group->source;

	BUG_ON(!source);
	BUG_ON(!snd_stm_magic_valid(source));

	snd_stm_conv_ctl_mute.device = source->card_device;

	ctl_mute = snd_ctl_new1(&snd_stm_conv_ctl_mute, converter);
	result = snd_ctl_add(source->card, ctl_mute);
	if (result >= 0) {
		 
		if (snd_stm_card_is_registered(SND_STM_CARD_TYPE_AUDIO))
			converter->ctl_mute = ctl_mute;

		snd_stm_conv_ctl_mute.index++;
	} else {
		snd_stm_printe("Error %d while adding mute ALSA control!\n",
				result);
	}

	return result;
}

static int snd_stm_conv_ctl_route_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	struct snd_stm_conv_source *source = snd_kcontrol_chip(kcontrol);
	struct snd_stm_conv_group *group;
	int item = 0;

	snd_stm_printd(1, "snd_stm_conv_ctl_route_info(kcontrol=0x%p,"
			" uinfo=0x%p)\n", kcontrol, uinfo);

	BUG_ON(!source);
	BUG_ON(!snd_stm_magic_valid(source));

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;

#ifdef MY_DEF_HERE
	spin_lock(&snd_stm_conv_lock);
#else  
	mutex_lock(&snd_stm_conv_mutex);
#endif  

	list_for_each_entry(group, &source->groups, list) {
		if (list_is_last(&group->list, &source->groups) &&
				uinfo->value.enumerated.item > item)
			uinfo->value.enumerated.item = item;
		if (item == uinfo->value.enumerated.item)
			snprintf(uinfo->value.enumerated.name, 64, "%s",
					group->name);
		item++;
	};

	uinfo->value.enumerated.items = item;

#ifdef MY_DEF_HERE
	spin_unlock(&snd_stm_conv_lock);
#else  
	mutex_unlock(&snd_stm_conv_mutex);
#endif  

	return 0;
}

static int snd_stm_conv_ctl_route_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_conv_source *source = snd_kcontrol_chip(kcontrol);
	struct snd_stm_conv_group *group;
	int item = 0;

	snd_stm_printd(1, "snd_stm_conv_ctl_route_get(kcontrol=0x%p,"
			" ucontrol=0x%p)\n", kcontrol, ucontrol);

	BUG_ON(!source);
	BUG_ON(!snd_stm_magic_valid(source));

#ifdef MY_DEF_HERE
	spin_lock(&snd_stm_conv_lock);
#else  
	mutex_lock(&snd_stm_conv_mutex);
#endif  

	ucontrol->value.enumerated.item[0] = 0;  

	list_for_each_entry(group, &source->groups, list) {
		if (group == source->group_selected) {
			ucontrol->value.enumerated.item[0] = item;
			break;
		}
		item++;
	};

#ifdef MY_DEF_HERE
	spin_unlock(&snd_stm_conv_lock);
#else  
	mutex_unlock(&snd_stm_conv_mutex);
#endif  

	return 0;
}

static int snd_stm_conv_ctl_route_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int changed = 0;
	struct snd_stm_conv_source *source = snd_kcontrol_chip(kcontrol);
	struct snd_stm_conv_group *group;
	int item = 0;

	snd_stm_printd(1, "snd_stm_conv_ctl_route_put(kcontrol=0x%p,"
			" ucontrol=0x%p)\n", kcontrol, ucontrol);

	BUG_ON(!source);
	BUG_ON(!snd_stm_magic_valid(source));

#ifdef MY_DEF_HERE
	spin_lock(&snd_stm_conv_lock);
#else  
	mutex_lock(&snd_stm_conv_mutex);
#endif  

	list_for_each_entry(group, &source->groups, list) {
		if (item == ucontrol->value.enumerated.item[0]) {
			if (group != source->group_selected) {
				changed = 1;
				source->group_selected = group;
			}
			break;
		}
		item++;
	}

#ifdef MY_DEF_HERE
	spin_unlock(&snd_stm_conv_lock);
#else  
	mutex_unlock(&snd_stm_conv_mutex);
#endif  

	return changed;
}

static struct snd_kcontrol_new snd_stm_conv_ctl_route = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "PCM Playback Route",
	.info = snd_stm_conv_ctl_route_info,
	.get = snd_stm_conv_ctl_route_get,
	.put = snd_stm_conv_ctl_route_put,
};

static int snd_stm_conv_ctl_route_add(struct snd_stm_conv_source *source)
{
	struct snd_kcontrol *ctl_route;
	int result;

	snd_stm_printd(1, "snd_stm_conv_ctl_route_add(source=%p)\n", source);

	BUG_ON(!source);
	BUG_ON(!snd_stm_magic_valid(source));

	snd_stm_conv_ctl_route.device = source->card_device;

	ctl_route = snd_ctl_new1(&snd_stm_conv_ctl_route, source);
	result = snd_ctl_add(source->card, ctl_route);
	if (result >= 0) {
		 
		if (snd_stm_card_is_registered(SND_STM_CARD_TYPE_AUDIO))
			source->ctl_route = ctl_route;

		snd_stm_conv_ctl_route.index++;
	} else {
		snd_stm_printe("Error %d while adding route ALSA "
				"control!\n", result);
	}

	return result;
}

static inline int snd_stm_conv_more_than_one_entry(const struct list_head *head)
{
	return !list_empty(head) && !list_is_last(head->next, head);
}

static struct snd_stm_conv_source *snd_stm_conv_get_source(
		struct bus_type *bus, struct device_node *bus_id)
{
	struct snd_stm_conv_source *source;

	snd_stm_printd(1, "snd_stm_conv_get_source(bus=%p, bus_id='%s')\n",
			bus, bus_id->full_name);

	BUG_ON(!bus);
	BUG_ON(!bus_id);

#ifdef MY_DEF_HERE
#else  
	mutex_lock(&snd_stm_conv_mutex);
#endif  

	list_for_each_entry(source, &snd_stm_conv_sources, list)
		if ((bus == source->bus) && (bus_id == source->bus_id))
			goto done;  

	source = kzalloc(sizeof(*source), GFP_KERNEL);
	if (!source) {
		snd_stm_printe("Can't allocate memory for source!\n");
		goto done;
	}
	snd_stm_magic_set(source);

	source->bus = bus;
	source->bus_id = bus_id;
	INIT_LIST_HEAD(&source->groups);

	list_add_tail(&source->list, &snd_stm_conv_sources);

done:
#ifdef MY_DEF_HERE
#else  
	mutex_unlock(&snd_stm_conv_mutex);
#endif  

	return source;
}

struct snd_stm_conv_source *snd_stm_conv_register_source(struct bus_type *bus,
		struct device_node *bus_id, int channels_num,
		struct snd_card *card, int card_device)
{
	struct snd_stm_conv_source *source;
	struct snd_stm_conv_group *group;

	snd_stm_printd(1, "snd_stm_conv_register_source(bus=%p, bus_id='%s', "
			"channels_num=%d, card=%p, card_device=%d)\n",
			bus, bus_id->full_name, channels_num, card,
			card_device);

	BUG_ON(!bus);
	BUG_ON(!bus_id);
	BUG_ON(channels_num <= 0);
	BUG_ON(!card);
	BUG_ON(card_device < 0);

	source = snd_stm_conv_get_source(bus, bus_id);
	if (!source) {
		snd_stm_printe("Can't get source structure!\n");
		return NULL;
	}

	BUG_ON(source->channels_num != 0);
	BUG_ON(source->card);

	source->channels_num = channels_num;
	source->card = card;
	source->card_device = card_device;

#ifdef MY_DEF_HERE
	spin_lock(&snd_stm_conv_lock);
#else  
	mutex_lock(&snd_stm_conv_mutex);
#endif  

	if (snd_stm_conv_more_than_one_entry(&source->groups) &&
			snd_stm_conv_ctl_route_add(source) != 0)
		snd_stm_printe("Failed to add route ALSA control!\n");

	list_for_each_entry(group, &source->groups, list) {
		struct snd_stm_conv_converter *converter;

		BUG_ON(!snd_stm_magic_valid(group));

		list_for_each_entry(converter, &group->converters, list) {
			BUG_ON(!snd_stm_magic_valid(converter));

			if (converter->ops->set_muted &&
					snd_stm_conv_ctl_mute_add(converter)
					< 0)
				snd_stm_printe("Failed to add mute "
						"ALSA control!\n");
		}
	}

#ifdef MY_DEF_HERE
	spin_unlock(&snd_stm_conv_lock);
#else  
	mutex_unlock(&snd_stm_conv_mutex);
#endif  

	return source;
}
EXPORT_SYMBOL(snd_stm_conv_register_source);

int snd_stm_conv_unregister_source(struct snd_stm_conv_source *source)
{
	snd_stm_printd(1, "snd_stm_conv_unregister_source(source=%p)\n",
		       source);

	BUG_ON(!source);
	BUG_ON(!snd_stm_magic_valid(source));

#ifdef MY_DEF_HERE
	spin_lock(&snd_stm_conv_lock);
#else  
	mutex_lock(&snd_stm_conv_mutex);
#endif  

	list_del(&source->list);
	source->channels_num = 0;

	if (list_empty(&source->groups)) {
		snd_stm_magic_clear(source);
		kfree(source);
	}

#ifdef MY_DEF_HERE
	spin_unlock(&snd_stm_conv_lock);
#else  
	mutex_unlock(&snd_stm_conv_mutex);
#endif  

	return 0;
}
EXPORT_SYMBOL(snd_stm_conv_unregister_source);

static struct snd_stm_conv_group *snd_stm_conv_get_group(
		struct snd_stm_conv_source *source, const char *name)
{
	struct snd_stm_conv_group *group;

	snd_stm_printd(1, "snd_stm_conv_get_group(source=%p, name='%s')\n",
			source, name);

	BUG_ON(!source);
	BUG_ON(!snd_stm_magic_valid(source));
	BUG_ON(!name);

	BUG_ON(strlen(name) > 1024);

#ifdef MY_DEF_HERE
	spin_lock(&snd_stm_conv_lock);
#else  
	mutex_lock(&snd_stm_conv_mutex);
#endif  

	list_for_each_entry(group, &source->groups, list)
		if (strcmp(name, group->name) == 0)
			goto done;  

	group = kzalloc(sizeof(*group) + strlen(name), GFP_KERNEL);
	if (!group) {
		snd_stm_printe("Can't allocate memory for group!\n");
		goto done;
	}
	snd_stm_magic_set(group);

	INIT_LIST_HEAD(&group->converters);

	strcpy(group->name, name);

	group->source = source;
	group->muted_by_source = 1;

	if (!source->group_selected)
		source->group_selected = group;

	list_add_tail(&group->list, &source->groups);

	if (source->card && snd_stm_conv_more_than_one_entry(&source->groups))
		snd_stm_conv_ctl_route_add(source);

done:
#ifdef MY_DEF_HERE
	spin_unlock(&snd_stm_conv_lock);
#else  
	mutex_unlock(&snd_stm_conv_mutex);
#endif  

	return group;
}

static int snd_stm_conv_remove_group(struct snd_stm_conv_group *group)
{
	struct snd_stm_conv_source *source;

	snd_stm_printd(1, "snd_stm_conv_remove_group(group=%p)\n", group);

	BUG_ON(!group);
	BUG_ON(!snd_stm_magic_valid(group));

	source = group->source;

	BUG_ON(!source);
	BUG_ON(!snd_stm_magic_valid(source));

	list_del(&group->list);

	if (group == source->group_active)
		snd_stm_printe("WARNING! Removing active converters group! "
				"I hope you know what are you doing...\n");

	if (group == source->group_selected) {
		if (list_empty(&source->groups)) {
			 
			source->group_selected = NULL;
		} else {
			 
			if (!snd_stm_conv_more_than_one_entry(&source->groups)
					&& source->ctl_route)
				snd_ctl_remove(source->card, source->ctl_route);

			source->group_selected = list_first_entry(
					&source->groups,
					struct snd_stm_conv_group, list);
		}
	}

	snd_stm_magic_clear(group);
	kfree(group);

	if (list_empty(&source->groups) && source->channels_num == 0) {
		snd_stm_magic_clear(source);
		kfree(source);
	}

	return 0;
}

struct snd_stm_conv_converter *snd_stm_conv_register_converter(
		const char *group_name, struct snd_stm_conv_ops *ops,
		void *priv,
		struct bus_type *source_bus, struct device_node *source_bus_id,
		int source_channel_from, int source_channel_to, int *index)
{
	static int number;
	struct snd_stm_conv_converter *converter;
	struct snd_stm_conv_group *group;
	struct snd_stm_conv_source *source;

	snd_stm_printd(1, "snd_stm_conv_register_converter(group='%s', ops=%p,"
			" priv=%p, source_bus=%p, source_bus_id='%s', "
			"source_channel_from=%d, source_channel_to=%d)\n",
			group_name, ops, priv, source_bus,
			source_bus_id->full_name, source_channel_from,
			source_channel_to);

	BUG_ON(!group_name);
	BUG_ON(!ops);
	BUG_ON(!source_bus);
	BUG_ON(!source_bus_id);
	BUG_ON(source_channel_from < 0);
	BUG_ON(source_channel_to < source_channel_from);

	converter = kzalloc(sizeof(*converter), GFP_KERNEL);
	if (!converter) {
		snd_stm_printe("Can't allocate memory for converter!\n");
		goto error_kzalloc;
	}
	snd_stm_magic_set(converter);

	converter->ops = ops;
	converter->priv = priv;
	converter->source_channel_from = source_channel_from;
	converter->source_channel_to = source_channel_to;

	converter->muted_by_source = 1;

	spin_lock_init(&converter->status_lock);

	source = snd_stm_conv_get_source(source_bus, source_bus_id);
	if (!source) {
		snd_stm_printe("Can't get source structure!\n");
		goto error_get_source;
	}

	group = snd_stm_conv_get_group(source, group_name);
	if (!group) {
		snd_stm_printe("Can't get group structure!\n");
		goto error_get_group;
	}

	if (source->group_active == group)
		snd_stm_printe("WARNING! Adding a converter to an active "
				"group!\n");

#ifdef MY_DEF_HERE
	spin_lock(&snd_stm_conv_lock);
#else  
	mutex_lock(&snd_stm_conv_mutex);
#endif  

	converter->group = group;
	list_add_tail(&converter->list, &group->converters);

#ifdef MY_DEF_HERE
	spin_unlock(&snd_stm_conv_lock);
#else  
	mutex_unlock(&snd_stm_conv_mutex);
#endif  

	if (source->card && ops->set_muted &&
			snd_stm_conv_ctl_mute_add(converter) < 0) {
		snd_stm_printe("Failed to add mute ALSA control!\n");
		goto error_add_ctl_mute;
	}

	number++;
	if (index)
		*index = number;

	return converter;

error_add_ctl_mute:
error_get_group:
error_get_source:
	list_del(&converter->list);
	snd_stm_magic_clear(converter);
	kfree(converter);
error_kzalloc:
	return NULL;
}
EXPORT_SYMBOL(snd_stm_conv_register_converter);

int snd_stm_conv_unregister_converter(struct snd_stm_conv_converter *converter)
{
	struct snd_stm_conv_group *group;

	snd_stm_printd(1, "snd_stm_conv_unregister_converter(converter=%p)\n",
			converter);

	BUG_ON(!converter);
	BUG_ON(!snd_stm_magic_valid(converter));

	group = converter->group;

	BUG_ON(!group);
	BUG_ON(!snd_stm_magic_valid(group));

#ifdef MY_DEF_HERE
	spin_lock(&snd_stm_conv_lock);
#else  
	mutex_lock(&snd_stm_conv_mutex);
#endif  

	list_del(&converter->list);

	if (converter->ctl_mute)
		snd_ctl_remove(group->source->card, converter->ctl_mute);

	snd_stm_magic_clear(converter);
	kfree(converter);

	if (list_empty(&group->converters))
		snd_stm_conv_remove_group(group);

#ifdef MY_DEF_HERE
	spin_unlock(&snd_stm_conv_lock);
#else  
	mutex_unlock(&snd_stm_conv_mutex);
#endif  

	return 0;
}
EXPORT_SYMBOL(snd_stm_conv_unregister_converter);

int snd_stm_conv_get_card_device(struct snd_stm_conv_converter *converter)
{
	snd_stm_printd(1, "snd_stm_conv_get_card_device(converter=%p)\n",
			converter);

	BUG_ON(!converter);
	BUG_ON(!snd_stm_magic_valid(converter));

	return converter->group->source->card_device;
}
EXPORT_SYMBOL(snd_stm_conv_get_card_device);

struct snd_stm_conv_group *snd_stm_conv_request_group(
		struct snd_stm_conv_source *source)
{
	snd_stm_printd(1, "snd_stm_conv_request_group(source=%p)\n", source);

	BUG_ON(!source);
	BUG_ON(!snd_stm_magic_valid(source));
	BUG_ON(source->group_active);

#ifdef MY_DEF_HERE
	spin_lock(&snd_stm_conv_lock);
#else  
	mutex_lock(&snd_stm_conv_mutex);
#endif  

	source->group_active = source->group_selected;

#ifdef MY_DEF_HERE
	spin_unlock(&snd_stm_conv_lock);
#else  
	mutex_unlock(&snd_stm_conv_mutex);
#endif  

	return source->group_active;
}
EXPORT_SYMBOL(snd_stm_conv_request_group);

int snd_stm_conv_release_group(struct snd_stm_conv_group *group)
{
	snd_stm_printd(1, "snd_stm_conv_release_group(group=%p)\n", group);

	BUG_ON(!group);
	BUG_ON(!snd_stm_magic_valid(group));
	BUG_ON(!group->source);
	BUG_ON(!snd_stm_magic_valid(group->source));
	BUG_ON(group != group->source->group_active);

#ifdef MY_DEF_HERE
	spin_lock(&snd_stm_conv_lock);
#else  
	mutex_lock(&snd_stm_conv_mutex);
#endif  

	group->source->group_active = NULL;

#ifdef MY_DEF_HERE
	spin_unlock(&snd_stm_conv_lock);
#else  
	mutex_unlock(&snd_stm_conv_mutex);
#endif  

	return 0;
}
EXPORT_SYMBOL(snd_stm_conv_release_group);

static struct snd_info_entry *snd_stm_conv_proc_entry;

static void snd_stm_conv_info(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_conv_source *source;

	snd_iprintf(buffer, "--- converters ---\n");

	list_for_each_entry(source, &snd_stm_conv_sources, list) {
		struct snd_stm_conv_group *group;

		snd_iprintf(buffer, "- source %p:\n", source);
		snd_iprintf(buffer, "  bus=%p, bus_id='%s'\n",
				source->bus, source->bus_id->full_name);
		snd_iprintf(buffer, "  channels_num=%d\n",
				source->channels_num);
		snd_iprintf(buffer, "  card=%p, card_device=%d\n",
				source->card, source->card_device);

		list_for_each_entry(group, &source->groups, list) {
			struct snd_stm_conv_converter *converter;

			snd_iprintf(buffer, "  - group %p:\n", group);
			snd_iprintf(buffer, "    name='%s'\n", group->name);

			list_for_each_entry(converter, &group->converters,
					list) {
				snd_iprintf(buffer, "    - converter %p:\n",
						converter);
				snd_iprintf(buffer, "      source_channel_from"
						"=%d, source_channel_to=%d\n",
						converter->source_channel_from,
						converter->source_channel_to);
				snd_iprintf(buffer, "      enabled=%d\n",
						converter->enabled);
				snd_iprintf(buffer, "      muted_by_source=%d,"
						" muted_by_user=%d\n",
						converter->muted_by_source,
						converter->muted_by_user);
			}
		}
	}
	snd_iprintf(buffer, "\n");
}

int snd_stm_conv_init(void)
{
	 
	snd_stm_info_register(&snd_stm_conv_proc_entry, "converters",
			snd_stm_conv_info, NULL);

	return 0;
}

void snd_stm_conv_exit(void)
{
	snd_stm_info_unregister(snd_stm_conv_proc_entry);

	if (!list_empty(&snd_stm_conv_sources)) {
		struct snd_stm_conv_source *source, *source_next;

		snd_stm_printe("WARNING! There are some converters "
				"infrastructure components left - "
				"check your configuration!\n");

		list_for_each_entry_safe(source, source_next,
				&snd_stm_conv_sources, list) {
			struct snd_stm_conv_group *group, *group_next;

			list_for_each_entry_safe(group, group_next,
					&source->groups, list) {
				struct snd_stm_conv_converter *converter,
						*converter_next;

				list_for_each_entry_safe(converter,
						converter_next,
						&group->converters, list) {
					list_del(&converter->list);
					snd_stm_magic_clear(converter);
					kfree(converter);
				}
				list_del(&group->list);
				snd_stm_magic_clear(group);
				kfree(group);
			}
			list_del(&source->list);
			snd_stm_magic_clear(source);
			kfree(source);
		}
	}
}
