var d1, d2: domain(1);

cobegin {
  on Locales(0) do d1 = {1..2};
  on Locales(1) do d2 = {3..4};
}

writeln((d1, d2));
