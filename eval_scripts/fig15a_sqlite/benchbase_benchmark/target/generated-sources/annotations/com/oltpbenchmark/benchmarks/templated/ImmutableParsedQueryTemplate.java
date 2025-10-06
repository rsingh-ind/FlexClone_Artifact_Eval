package com.oltpbenchmark.benchmarks.templated;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Objects;
import org.immutables.value.Generated;

/**
 * Immutable implementation of {@link TemplatedBenchmark.ParsedQueryTemplate}.
 * <p>
 * Use the builder to create immutable instances:
 * {@code ImmutableParsedQueryTemplate.builder()}.
 */
@Generated(from = "TemplatedBenchmark.ParsedQueryTemplate", generator = "Immutables")
@SuppressWarnings({"all"})
@javax.annotation.processing.Generated("org.immutables.processor.ProxyProcessor")
public final class ImmutableParsedQueryTemplate
    implements TemplatedBenchmark.ParsedQueryTemplate {
  private final String name;
  private final String query;
  private final List<String> paramsTypes;
  private final List<String> paramsValues;

  private ImmutableParsedQueryTemplate(ImmutableParsedQueryTemplate.Builder builder) {
    this.name = builder.name;
    this.query = builder.query;
    if (builder.paramsTypesIsSet()) {
      initShim.paramsTypes(createUnmodifiableList(true, builder.paramsTypes));
    }
    if (builder.paramsValuesIsSet()) {
      initShim.paramsValues(createUnmodifiableList(true, builder.paramsValues));
    }
    this.paramsTypes = initShim.getParamsTypes();
    this.paramsValues = initShim.getParamsValues();
    this.initShim = null;
  }

  private ImmutableParsedQueryTemplate(
      String name,
      String query,
      List<String> paramsTypes,
      List<String> paramsValues) {
    this.name = name;
    this.query = query;
    this.paramsTypes = paramsTypes;
    this.paramsValues = paramsValues;
    this.initShim = null;
  }

  private static final byte STAGE_INITIALIZING = -1;
  private static final byte STAGE_UNINITIALIZED = 0;
  private static final byte STAGE_INITIALIZED = 1;
  private transient volatile InitShim initShim = new InitShim();

  @Generated(from = "TemplatedBenchmark.ParsedQueryTemplate", generator = "Immutables")
  private final class InitShim {
    private byte paramsTypesBuildStage = STAGE_UNINITIALIZED;
    private List<String> paramsTypes;

    List<String> getParamsTypes() {
      if (paramsTypesBuildStage == STAGE_INITIALIZING) throw new IllegalStateException(formatInitCycleMessage());
      if (paramsTypesBuildStage == STAGE_UNINITIALIZED) {
        paramsTypesBuildStage = STAGE_INITIALIZING;
        this.paramsTypes = createUnmodifiableList(false, createSafeList(getParamsTypesInitialize(), true, false));
        paramsTypesBuildStage = STAGE_INITIALIZED;
      }
      return this.paramsTypes;
    }

    void paramsTypes(List<String> paramsTypes) {
      this.paramsTypes = paramsTypes;
      paramsTypesBuildStage = STAGE_INITIALIZED;
    }

    private byte paramsValuesBuildStage = STAGE_UNINITIALIZED;
    private List<String> paramsValues;

    List<String> getParamsValues() {
      if (paramsValuesBuildStage == STAGE_INITIALIZING) throw new IllegalStateException(formatInitCycleMessage());
      if (paramsValuesBuildStage == STAGE_UNINITIALIZED) {
        paramsValuesBuildStage = STAGE_INITIALIZING;
        this.paramsValues = createUnmodifiableList(false, createSafeList(getParamsValuesInitialize(), true, false));
        paramsValuesBuildStage = STAGE_INITIALIZED;
      }
      return this.paramsValues;
    }

    void paramsValues(List<String> paramsValues) {
      this.paramsValues = paramsValues;
      paramsValuesBuildStage = STAGE_INITIALIZED;
    }

    private String formatInitCycleMessage() {
      List<String> attributes = new ArrayList<>();
      if (paramsTypesBuildStage == STAGE_INITIALIZING) attributes.add("paramsTypes");
      if (paramsValuesBuildStage == STAGE_INITIALIZING) attributes.add("paramsValues");
      return "Cannot build ParsedQueryTemplate, attribute initializers form cycle " + attributes;
    }
  }

  private List<String> getParamsTypesInitialize() {
    return TemplatedBenchmark.ParsedQueryTemplate.super.getParamsTypes();
  }

  private List<String> getParamsValuesInitialize() {
    return TemplatedBenchmark.ParsedQueryTemplate.super.getParamsValues();
  }

  /**
   *Template name. 
   */
  @Override
  public String getName() {
    return name;
  }

  /**
   *Query string for this template. 
   */
  @Override
  public String getQuery() {
    return query;
  }

  /**
   *Potential query parameter types. 
   */
  @Override
  public List<String> getParamsTypes() {
    InitShim shim = this.initShim;
    return shim != null
        ? shim.getParamsTypes()
        : this.paramsTypes;
  }

  /**
   *Potential query parameter values. 
   */
  @Override
  public List<String> getParamsValues() {
    InitShim shim = this.initShim;
    return shim != null
        ? shim.getParamsValues()
        : this.paramsValues;
  }

  /**
   * Copy the current immutable object by setting a value for the {@link TemplatedBenchmark.ParsedQueryTemplate#getName() name} attribute.
   * An equals check used to prevent copying of the same value by returning {@code this}.
   * @param value A new value for name
   * @return A modified copy of the {@code this} object
   */
  public final ImmutableParsedQueryTemplate withName(String value) {
    String newValue = Objects.requireNonNull(value, "name");
    if (this.name.equals(newValue)) return this;
    return new ImmutableParsedQueryTemplate(newValue, this.query, this.paramsTypes, this.paramsValues);
  }

  /**
   * Copy the current immutable object by setting a value for the {@link TemplatedBenchmark.ParsedQueryTemplate#getQuery() query} attribute.
   * An equals check used to prevent copying of the same value by returning {@code this}.
   * @param value A new value for query
   * @return A modified copy of the {@code this} object
   */
  public final ImmutableParsedQueryTemplate withQuery(String value) {
    String newValue = Objects.requireNonNull(value, "query");
    if (this.query.equals(newValue)) return this;
    return new ImmutableParsedQueryTemplate(this.name, newValue, this.paramsTypes, this.paramsValues);
  }

  /**
   * Copy the current immutable object with elements that replace the content of {@link TemplatedBenchmark.ParsedQueryTemplate#getParamsTypes() paramsTypes}.
   * @param elements The elements to set
   * @return A modified copy of {@code this} object
   */
  public final ImmutableParsedQueryTemplate withParamsTypes(String... elements) {
    List<String> newValue = createUnmodifiableList(false, createSafeList(Arrays.asList(elements), true, false));
    return new ImmutableParsedQueryTemplate(this.name, this.query, newValue, this.paramsValues);
  }

  /**
   * Copy the current immutable object with elements that replace the content of {@link TemplatedBenchmark.ParsedQueryTemplate#getParamsTypes() paramsTypes}.
   * A shallow reference equality check is used to prevent copying of the same value by returning {@code this}.
   * @param elements An iterable of paramsTypes elements to set
   * @return A modified copy of {@code this} object
   */
  public final ImmutableParsedQueryTemplate withParamsTypes(Iterable<String> elements) {
    if (this.paramsTypes == elements) return this;
    List<String> newValue = createUnmodifiableList(false, createSafeList(elements, true, false));
    return new ImmutableParsedQueryTemplate(this.name, this.query, newValue, this.paramsValues);
  }

  /**
   * Copy the current immutable object with elements that replace the content of {@link TemplatedBenchmark.ParsedQueryTemplate#getParamsValues() paramsValues}.
   * @param elements The elements to set
   * @return A modified copy of {@code this} object
   */
  public final ImmutableParsedQueryTemplate withParamsValues(String... elements) {
    List<String> newValue = createUnmodifiableList(false, createSafeList(Arrays.asList(elements), true, false));
    return new ImmutableParsedQueryTemplate(this.name, this.query, this.paramsTypes, newValue);
  }

  /**
   * Copy the current immutable object with elements that replace the content of {@link TemplatedBenchmark.ParsedQueryTemplate#getParamsValues() paramsValues}.
   * A shallow reference equality check is used to prevent copying of the same value by returning {@code this}.
   * @param elements An iterable of paramsValues elements to set
   * @return A modified copy of {@code this} object
   */
  public final ImmutableParsedQueryTemplate withParamsValues(Iterable<String> elements) {
    if (this.paramsValues == elements) return this;
    List<String> newValue = createUnmodifiableList(false, createSafeList(elements, true, false));
    return new ImmutableParsedQueryTemplate(this.name, this.query, this.paramsTypes, newValue);
  }

  /**
   * This instance is equal to all instances of {@code ImmutableParsedQueryTemplate} that have equal attribute values.
   * @return {@code true} if {@code this} is equal to {@code another} instance
   */
  @Override
  public boolean equals(Object another) {
    if (this == another) return true;
    return another instanceof ImmutableParsedQueryTemplate
        && equalTo(0, (ImmutableParsedQueryTemplate) another);
  }

  private boolean equalTo(int synthetic, ImmutableParsedQueryTemplate another) {
    return name.equals(another.name)
        && query.equals(another.query)
        && paramsTypes.equals(another.paramsTypes)
        && paramsValues.equals(another.paramsValues);
  }

  /**
   * Computes a hash code from attributes: {@code name}, {@code query}, {@code paramsTypes}, {@code paramsValues}.
   * @return hashCode value
   */
  @Override
  public int hashCode() {
    int h = 5381;
    h += (h << 5) + name.hashCode();
    h += (h << 5) + query.hashCode();
    h += (h << 5) + paramsTypes.hashCode();
    h += (h << 5) + paramsValues.hashCode();
    return h;
  }

  /**
   * Prints the immutable value {@code ParsedQueryTemplate} with attribute values.
   * @return A string representation of the value
   */
  @Override
  public String toString() {
    return "ParsedQueryTemplate{"
        + "name=" + name
        + ", query=" + query
        + ", paramsTypes=" + paramsTypes
        + ", paramsValues=" + paramsValues
        + "}";
  }

  /**
   * Creates an immutable copy of a {@link TemplatedBenchmark.ParsedQueryTemplate} value.
   * Uses accessors to get values to initialize the new immutable instance.
   * If an instance is already immutable, it is returned as is.
   * @param instance The instance to copy
   * @return A copied immutable ParsedQueryTemplate instance
   */
  public static ImmutableParsedQueryTemplate copyOf(TemplatedBenchmark.ParsedQueryTemplate instance) {
    if (instance instanceof ImmutableParsedQueryTemplate) {
      return (ImmutableParsedQueryTemplate) instance;
    }
    return ImmutableParsedQueryTemplate.builder()
        .from(instance)
        .build();
  }

  /**
   * Creates a builder for {@link ImmutableParsedQueryTemplate ImmutableParsedQueryTemplate}.
   * <pre>
   * ImmutableParsedQueryTemplate.builder()
   *    .name(String) // required {@link TemplatedBenchmark.ParsedQueryTemplate#getName() name}
   *    .query(String) // required {@link TemplatedBenchmark.ParsedQueryTemplate#getQuery() query}
   *    .addParamsTypes|addAllParamsTypes(String) // {@link TemplatedBenchmark.ParsedQueryTemplate#getParamsTypes() paramsTypes} elements
   *    .addParamsValues|addAllParamsValues(String) // {@link TemplatedBenchmark.ParsedQueryTemplate#getParamsValues() paramsValues} elements
   *    .build();
   * </pre>
   * @return A new ImmutableParsedQueryTemplate builder
   */
  public static ImmutableParsedQueryTemplate.Builder builder() {
    return new ImmutableParsedQueryTemplate.Builder();
  }

  /**
   * Builds instances of type {@link ImmutableParsedQueryTemplate ImmutableParsedQueryTemplate}.
   * Initialize attributes and then invoke the {@link #build()} method to create an
   * immutable instance.
   * <p><em>{@code Builder} is not thread-safe and generally should not be stored in a field or collection,
   * but instead used immediately to create instances.</em>
   */
  @Generated(from = "TemplatedBenchmark.ParsedQueryTemplate", generator = "Immutables")
  public static final class Builder {
    private static final long INIT_BIT_NAME = 0x1L;
    private static final long INIT_BIT_QUERY = 0x2L;
    private static final long OPT_BIT_PARAMS_TYPES = 0x1L;
    private static final long OPT_BIT_PARAMS_VALUES = 0x2L;
    private long initBits = 0x3L;
    private long optBits;

    private String name;
    private String query;
    private List<String> paramsTypes = new ArrayList<String>();
    private List<String> paramsValues = new ArrayList<String>();

    private Builder() {
    }

    /**
     * Fill a builder with attribute values from the provided {@code ParsedQueryTemplate} instance.
     * Regular attribute values will be replaced with those from the given instance.
     * Absent optional values will not replace present values.
     * Collection elements and entries will be added, not replaced.
     * @param instance The instance from which to copy values
     * @return {@code this} builder for use in a chained invocation
     */
    public final Builder from(TemplatedBenchmark.ParsedQueryTemplate instance) {
      Objects.requireNonNull(instance, "instance");
      this.name(instance.getName());
      this.query(instance.getQuery());
      addAllParamsTypes(instance.getParamsTypes());
      addAllParamsValues(instance.getParamsValues());
      return this;
    }

    /**
     * Initializes the value for the {@link TemplatedBenchmark.ParsedQueryTemplate#getName() name} attribute.
     * @param name The value for name 
     * @return {@code this} builder for use in a chained invocation
     */
    public final Builder name(String name) {
      this.name = Objects.requireNonNull(name, "name");
      initBits &= ~INIT_BIT_NAME;
      return this;
    }

    /**
     * Initializes the value for the {@link TemplatedBenchmark.ParsedQueryTemplate#getQuery() query} attribute.
     * @param query The value for query 
     * @return {@code this} builder for use in a chained invocation
     */
    public final Builder query(String query) {
      this.query = Objects.requireNonNull(query, "query");
      initBits &= ~INIT_BIT_QUERY;
      return this;
    }

    /**
     * Adds one element to {@link TemplatedBenchmark.ParsedQueryTemplate#getParamsTypes() paramsTypes} list.
     * @param element A paramsTypes element
     * @return {@code this} builder for use in a chained invocation
     */
    public final Builder addParamsTypes(String element) {
      this.paramsTypes.add(Objects.requireNonNull(element, "paramsTypes element"));
      optBits |= OPT_BIT_PARAMS_TYPES;
      return this;
    }

    /**
     * Adds elements to {@link TemplatedBenchmark.ParsedQueryTemplate#getParamsTypes() paramsTypes} list.
     * @param elements An array of paramsTypes elements
     * @return {@code this} builder for use in a chained invocation
     */
    public final Builder addParamsTypes(String... elements) {
      for (String element : elements) {
        this.paramsTypes.add(Objects.requireNonNull(element, "paramsTypes element"));
      }
      optBits |= OPT_BIT_PARAMS_TYPES;
      return this;
    }


    /**
     * Sets or replaces all elements for {@link TemplatedBenchmark.ParsedQueryTemplate#getParamsTypes() paramsTypes} list.
     * @param elements An iterable of paramsTypes elements
     * @return {@code this} builder for use in a chained invocation
     */
    public final Builder paramsTypes(Iterable<String> elements) {
      this.paramsTypes.clear();
      return addAllParamsTypes(elements);
    }

    /**
     * Adds elements to {@link TemplatedBenchmark.ParsedQueryTemplate#getParamsTypes() paramsTypes} list.
     * @param elements An iterable of paramsTypes elements
     * @return {@code this} builder for use in a chained invocation
     */
    public final Builder addAllParamsTypes(Iterable<String> elements) {
      for (String element : elements) {
        this.paramsTypes.add(Objects.requireNonNull(element, "paramsTypes element"));
      }
      optBits |= OPT_BIT_PARAMS_TYPES;
      return this;
    }

    /**
     * Adds one element to {@link TemplatedBenchmark.ParsedQueryTemplate#getParamsValues() paramsValues} list.
     * @param element A paramsValues element
     * @return {@code this} builder for use in a chained invocation
     */
    public final Builder addParamsValues(String element) {
      this.paramsValues.add(Objects.requireNonNull(element, "paramsValues element"));
      optBits |= OPT_BIT_PARAMS_VALUES;
      return this;
    }

    /**
     * Adds elements to {@link TemplatedBenchmark.ParsedQueryTemplate#getParamsValues() paramsValues} list.
     * @param elements An array of paramsValues elements
     * @return {@code this} builder for use in a chained invocation
     */
    public final Builder addParamsValues(String... elements) {
      for (String element : elements) {
        this.paramsValues.add(Objects.requireNonNull(element, "paramsValues element"));
      }
      optBits |= OPT_BIT_PARAMS_VALUES;
      return this;
    }


    /**
     * Sets or replaces all elements for {@link TemplatedBenchmark.ParsedQueryTemplate#getParamsValues() paramsValues} list.
     * @param elements An iterable of paramsValues elements
     * @return {@code this} builder for use in a chained invocation
     */
    public final Builder paramsValues(Iterable<String> elements) {
      this.paramsValues.clear();
      return addAllParamsValues(elements);
    }

    /**
     * Adds elements to {@link TemplatedBenchmark.ParsedQueryTemplate#getParamsValues() paramsValues} list.
     * @param elements An iterable of paramsValues elements
     * @return {@code this} builder for use in a chained invocation
     */
    public final Builder addAllParamsValues(Iterable<String> elements) {
      for (String element : elements) {
        this.paramsValues.add(Objects.requireNonNull(element, "paramsValues element"));
      }
      optBits |= OPT_BIT_PARAMS_VALUES;
      return this;
    }

    /**
     * Builds a new {@link ImmutableParsedQueryTemplate ImmutableParsedQueryTemplate}.
     * @return An immutable instance of ParsedQueryTemplate
     * @throws java.lang.IllegalStateException if any required attributes are missing
     */
    public ImmutableParsedQueryTemplate build() {
      if (initBits != 0) {
        throw new IllegalStateException(formatRequiredAttributesMessage());
      }
      return new ImmutableParsedQueryTemplate(this);
    }

    private boolean paramsTypesIsSet() {
      return (optBits & OPT_BIT_PARAMS_TYPES) != 0;
    }

    private boolean paramsValuesIsSet() {
      return (optBits & OPT_BIT_PARAMS_VALUES) != 0;
    }

    private String formatRequiredAttributesMessage() {
      List<String> attributes = new ArrayList<>();
      if ((initBits & INIT_BIT_NAME) != 0) attributes.add("name");
      if ((initBits & INIT_BIT_QUERY) != 0) attributes.add("query");
      return "Cannot build ParsedQueryTemplate, some of required attributes are not set " + attributes;
    }
  }

  private static <T> List<T> createSafeList(Iterable<? extends T> iterable, boolean checkNulls, boolean skipNulls) {
    ArrayList<T> list;
    if (iterable instanceof Collection<?>) {
      int size = ((Collection<?>) iterable).size();
      if (size == 0) return Collections.emptyList();
      list = new ArrayList<>(size);
    } else {
      list = new ArrayList<>();
    }
    for (T element : iterable) {
      if (skipNulls && element == null) continue;
      if (checkNulls) Objects.requireNonNull(element, "element");
      list.add(element);
    }
    return list;
  }

  private static <T> List<T> createUnmodifiableList(boolean clone, List<T> list) {
    switch(list.size()) {
    case 0: return Collections.emptyList();
    case 1: return Collections.singletonList(list.get(0));
    default:
      if (clone) {
        return Collections.unmodifiableList(new ArrayList<>(list));
      } else {
        if (list instanceof ArrayList<?>) {
          ((ArrayList<?>) list).trimToSize();
        }
        return Collections.unmodifiableList(list);
      }
    }
  }
}
