<script setup lang="ts">
  import { inject, onMounted, ref, type Ref } from 'vue';
  import IconEcosystem from '../icons/IconEcosystem.vue';
  import { GenericDriverStatus } from '../../model/GenericDriver';
  import type { Axios } from 'axios';

  const axios: Axios | undefined = inject('axios');

  let driverStatus: Ref<GenericDriverStatus | undefined> = ref(undefined);

  const getStatus = () => {
    axios?.get<GenericDriverStatus>('http://192.168.9.133/driver')
      .then(res => {
        console.log('XHR result: ', res);
        driverStatus.value = res.data;
      });
  }
  
  onMounted(() => {
    // driverStatus = { drv1: 1, drv2: 1}
    getStatus();
  });

  const switchOn = (event: Event) => {
    axios?.post('http://192.168.9.133/driver',
        {
          drv1: 2,
          drv2: 1
        })
      .then(res => {
        console.log('XHR result: ', res);
        driverStatus.value = res.data;
      });
  }
  const switchOff = (event: Event) => {
    axios?.post('http://192.168.9.133/driver',
        {
          drv1: 1,
          drv2: 1
        })
      .then(res => {
        console.log('XHR result: ', res);
        driverStatus.value = res.data;
      });
  }

</script>

<style scoped>
  .device {
    display: flex;
    flex-direction: column;
    margin: 16px;
  }

  .item-line {
    margin-top: 4px;
  }

  .item-cell {
    margin-right: 8px;
  }

  .item-line.actions {
    margin-top: 8px;
    display: flex;
  }


</style>

<template>
  <div class="device list-item">
    <div class="icon item-line"><IconEcosystem/></div>
    <div class="title item-line">Laistymo čiaupas rūsyje</div>
    <div class="state item-line">
        <span class="state-title item-cell">{{ driverStatus?.drv1 ? driverStatus.drv1 === 2 ? 'Įjungta' : 'Išjungta' : '-' }}</span>
        <!--<span class="state-desc item-cell">Suplanuota: 21:15</span>-->
    </div>
    <div class="actions item-line">
      <button v-if="driverStatus?.drv1 === 1" class="item-cell" @click="switchOn">Įjungti</button>
      <button v-if="driverStatus?.drv1 === 2" class="item-cell" @click="switchOff">Išjungti</button>
    </div>
  </div>
</template>
