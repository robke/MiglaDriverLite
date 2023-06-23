<script setup lang="ts">
import GenericDriverItem from '../devices/GenericDriverItem.vue';
import CatItem from './CatItem.vue';

</script>

<template>
  <div class="list">
    <CatItem>
        <GenericDriverItem/>
    </CatItem>
    <CatItem/>
    <CatItem/>
    <CatItem/>
    <CatItem/>
  </div>
</template>
